#!/usr/bin/perl
use strict;
use warnings;
use LWP::Simple qw(get);

sub check_cmd {
    my ($cmd, $msg) = @_;
    system($cmd);
    if ($? != 0) {
        die "ERROR: $msg\n";
    }
}

print "Checking network...\n";

# 1. Check if an interface is up
check_cmd("ip link show | grep 'state UP' > /dev/null",
          "No active network interface found");

print "Interface OK\n";

# 2. Check default gateway
my $gw = `ip route | grep default | awk '{print \$3}'`;
chomp $gw;

if (!$gw) {
    die "ERROR: No default gateway found\n";
}

check_cmd("ping -c1 -W2 $gw > /dev/null",
          "Gateway $gw unreachable");

print "Gateway reachable ($gw)\n";

# 3. Check internet connectivity
check_cmd("ping -c1 -W2 8.8.8.8 > /dev/null",
          "Internet unreachable (8.8.8.8)");

print "Internet connectivity OK\n";

# 4. DNS check
my $dns = `getent hosts example.com`;
if (!$dns) {
    die "ERROR: DNS resolution failed\n";
}

print "DNS working\n";

# 5. HTTP test
my $content = get("https://example.com");

if (!defined $content) {
    die "ERROR: HTTP request failed\n";
}

print "HTTP access OK\n";

print "Network status: OK\n";



#use strict;
#use warnings;
use LWP::Simple qw(get);

sub fail {
    my ($msg) = @_;
    print "ERROR: $msg\n";
    exit(1);
}

sub run_check {
    my ($cmd, $msg) = @_;
    system("$cmd > /dev/null 2>&1");
    if ($? != 0) {
        fail($msg);
    }
}

print "Checking VM network and HTTP service...\n";

# 1. Check VM has correct subnet
run_check("ip addr | grep '10.10.10.'",
          "VM does not have a 10.10.10.x address");

print "IP address OK\n";

# 2. Check port 80 is listening
run_check("ss -ltn | grep ':80 '",
          "No service listening on port 80");

print "Port 80 listening\n";

# 3. Check HTTP locally
$content = get("http://127.0.0.1");

if (!defined $content) {
    fail("Local HTTP service not responding");
}

print "Local HTTP OK\n";

# 4. Check HTTP via VM IP
my $ip = `hostname -I | awk '{print \$1}'`;
chomp $ip;

my $remote = get("http://$ip");

if (!defined $remote) {
    fail("HTTP service not reachable via VM IP ($ip)");
}

print "HTTP via VM IP OK ($ip)\n";

print "Network + HTTP status: OK\n";

my @IPs = ("10.10.10.1", "10.10.10.2", "10.10.10.3");
foreach my $szIP (@IPs) {
    my $szUrl = "http://$szIP";
    my $remote = get($szUrl);
    if (!defined $remote) {
        print "$szUrl HTTP service not responding\n";
    } else {
        print "$szUrl is responding\n";
    }
}

#******************* Check wifi on routervm ********************

use strict;
use warnings;

sub fail2 {
    my ($msg) = @_;
    die "ERROR: $msg\n";
}

sub run_cmd {
    my ($cmd) = @_;
    my $out = `$cmd 2>/dev/null`;
    chomp $out;
    return $out;
}

sub check_ok {
    my ($cmd, $msg) = @_;
    system("$cmd >/dev/null 2>&1");
    if ($? != 0) {
        fail($msg);
    }
}

print "Checking network status...\n";

# Get first non-loopback if normally the wan0.... check specifically lan0
#$ip = run_cmd(q(ip -4 addr show | awk '/inet / && $2 !~ /^127\./ {sub(/\/.*/, "", $2); print $2; exit}'));
$ip = run_cmd(q(ip -4 addr show dev lan0 | awk '/inet / {sub(/\/.*/, "", $2); print $2; exit}'));

if (!$ip) {
    fail("lan0 has no IPv4 address");
}

print "Detected lan0 IP: $ip\n";

print "Detected IP: $ip\n";

# Only do hotspot checks on router node
if ($ip eq '10.10.10.1') {
    print "This is router node 10.10.10.1, checking hotspot setup...\n";

    # Find wireless interface
    my $wifi_if = run_cmd(q(iw dev 2>/dev/null | awk '$1=="Interface"{print $2; exit}'));
    if (!$wifi_if) {
        fail("No wireless interface found");
    }
    print "Wireless interface: $wifi_if\n";

    # Check hostapd
    check_ok("systemctl is-active --quiet hostapd", "hostapd is not running");
    print "hostapd OK\n";

    # Check DHCP server
    check_ok("systemctl is-active --quiet isc-dhcp-server", "isc-dhcp-server is not running");
    print "DHCP server OK\n";

    # Check wireless interface has 10.10.10.1
    my $wifi_ip = run_cmd("ip -4 addr show dev $wifi_if | awk '/inet / {sub(/\\/.*/, \"\", \$2); print \$2; exit}'");
    if (!$wifi_ip) {
        fail("Wireless interface $wifi_if has no IPv4 address");
    }
    if ($wifi_ip ne '10.10.10.1') {
        fail("Wireless interface $wifi_if has wrong IP ($wifi_ip), expected 10.10.10.1");
    }
    print "Wireless interface IP OK\n";

    # Check IP forwarding
    my $forward = run_cmd(q(cat /proc/sys/net/ipv4/ip_forward));
    if ($forward ne '1') {
        fail("IPv4 forwarding is disabled");
    }
    print "IP forwarding OK\n";

    # Optional: check hostapd config exists
    check_ok("test -f /etc/hostapd/hostapd.conf", "Missing /etc/hostapd/hostapd.conf");
    print "hostapd config exists\n";

    print "Hotspot setup looks OK\n";
}
else {
    print "IP is not 10.10.10.1, skipping hotspot check\n";
}

print "All checks passed\n";
