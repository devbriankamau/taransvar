#!/usr/bin/env perl
use strict;
use warnings;
use POSIX qw(strftime);
use Getopt::Long qw(GetOptions);

# -----------------------------
# Options
# -----------------------------
my $conf = "/etc/hostapd/hostapd.conf";
my $iface = "";          # optional, only used for pre-checks/display
my $extra = "";          # extra args to pass to hostapd (advanced)
my $logdir = "/var/log";
my $no_prechecks = 0;

GetOptions(
    "conf=s"        => \$conf,
    "iface=s"       => \$iface,
    "extra=s"       => \$extra,
    "logdir=s"      => \$logdir,
    "no-prechecks!" => \$no_prechecks,
) or die "Usage: $0 [--conf /path/to/hostapd.conf] [--iface wlan0] [--extra '...'] [--logdir /path] [--no-prechecks]\n";

# -----------------------------
# Helpers
# -----------------------------
sub now_str { strftime("%Y-%m-%d %H:%M:%S", localtime) }
sub safe_run {
    my ($cmd) = @_;
    print "\n==> $cmd\n";
    system($cmd);
}
sub which {
    my ($bin) = @_;
    for my $d (split /:/, $ENV{PATH}) {
        my $p = "$d/$bin";
        return $p if -x $p;
    }
    return "";
}

# Must be root for full debugging (nl80211, interface ops, etc.)
if ($> != 0) {
    die "Run as root: sudo $0 --conf $conf\n";
}

my $hostapd = which("hostapd");
die "hostapd not found in PATH. Install it: sudo apt install hostapd\n" unless $hostapd;

die "Config not found: $conf\n" unless -f $conf;

my $ts = strftime("%Y%m%d-%H%M%S", localtime);
my $logfile = "$logdir/hostapd-debug-$ts.log";

# -----------------------------
# Pre-checks: quick system snapshot
# -----------------------------
if (!$no_prechecks) {
    print "Hostapd debug runner\n";
    print "Time: " . now_str() . "\n";
    print "hostapd: $hostapd\n";
    print "conf: $conf\n";
    print "log: $logfile\n";
    print "iface (optional): " . ($iface || "(not specified)") . "\n";

    safe_run("uname -a");
    safe_run("lsb_release -a 2>/dev/null || cat /etc/os-release");
    safe_run("rfkill list || true");
    safe_run("iw dev || true");
    safe_run("ip -br link || true");
    safe_run("ip -br addr || true");

    # Show interface details if given
    if ($iface) {
        safe_run("iw dev $iface info || true");
        safe_run("ethtool -i $iface 2>/dev/null || true");
        safe_run("nmcli dev status 2>/dev/null || true");
        safe_run("nmcli dev show $iface 2>/dev/null || true");
    } else {
        safe_run("nmcli dev status 2>/dev/null || true");
    }

    # hostapd unit state (informational)
    safe_run("systemctl status hostapd --no-pager 2>/dev/null || true");
    safe_run("systemctl is-enabled hostapd 2>/dev/null || true");
    safe_run("systemctl is-active hostapd 2>/dev/null || true");

    # Dmesg wifi-related hints (last 200 lines)
    safe_run("dmesg -T | tail -n 200 | egrep -i 'wlan|wifi|iwlwifi|ath|brcm|rtw|cfg80211|nl80211|firmware|regdom|rfkill|hostapd' || true");
}

# -----------------------------
# Run hostapd in foreground with maximum verbosity
# -----------------------------
print "\nStarting hostapd in foreground with debug...\n";
print "Log file: $logfile\n";
print "Press Ctrl+C to stop.\n";

# Many builds support:
#   -dd (more debug), -K (do not update keys?), -t (timestamps), -f (logfile)
# We prefer stdout capture for analysis, plus -t if available.
# We'll use: -dd -t and always capture to logfile.
my @cmd = ($hostapd, "-dd", "-t", $conf);

# Allow passing extra hostapd args (be careful!)
if ($extra) {
    push @cmd, split(/\s+/, $extra);
}

# Run and tee output with timestamps (Perl-based tee)
open my $LOG, ">>", $logfile or die "Cannot write $logfile: $!\n";

# Start process
my $pid = open(my $PH, "-|", @cmd);
die "Failed to run hostapd: $!\n" unless defined $pid;

# Common patterns to call out
my @patterns = (
    [ qr/nl80211: Driver does not support AP mode/i, "Driver/firmware does not support AP mode (or wrong driver= in config). Try driver=nl80211 and check your chipset supports AP." ],
    [ qr/could not configure driver mode/i, "Interface couldn't be put into AP mode. Often NetworkManager/wpa_supplicant is holding it, or interface name mismatch." ],
    [ qr/failed to set beacon parameters/i, "Beacon setup failed: regulatory/channel/HT settings or driver limitation. Try a simpler config (2.4GHz, channel 1/6/11) and check country_code." ],
    [ qr/AP-DISABLED/i, "AP disabled due to a fatal config/driver issue. Look for the error line above it." ],
    [ qr/Unable to open control interface/i, "Control interface issue: check ctrl_interface path/permissions or disable it temporarily." ],
    [ qr/Configuration file: (.+)/i, "Hostapd is reading a config file (confirm it's the expected one)." ],
    [ qr/Line (\d+): invalid/i, "Config parse error. Check the referenced line number in your conf." ],
    [ qr/ACS: Unable to collect survey data/i, "ACS (auto channel select) not supported. Disable ACS (set a fixed channel) or adjust hw_mode/channel." ],
    [ qr/DFS.*required/i, "You picked a DFS channel in 5GHz. Try a non-DFS channel or disable 5GHz while testing." ],
    [ qr/Interface initialization failed/i, "Interface init failed. Usually: wrong interface, in use by NM, rfkill, missing nl80211, bad channel/regdom." ],
    [ qr/ctrl_iface exists and seems to be in use/i, "Another hostapd is already running or stale ctrl_interface socket exists." ],
);

# Read output stream
while (my $line = <$PH>) {
    my $stamp = "[" . now_str() . "] ";
    print $stamp . $line;
    print $LOG $stamp . $line;

    for my $p (@patterns) {
        my ($re, $hint) = @$p;
        if ($line =~ $re) {
            my $hint_line = $stamp . "HINT: $hint\n";
            print $hint_line;
            print $LOG $hint_line;
        }
    }
}

close $PH;
close $LOG;

print "\nHostapd exited. Log saved to: $logfile\n";
exit 0;