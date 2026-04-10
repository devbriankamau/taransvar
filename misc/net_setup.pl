#!/usr/bin/perl
#net_setup.pl
use strict;
use warnings;
use FindBin;

my $conf_file = "/root/taransvar/net_setup.conf";
my $out_file  = "/root/taransvar/iptables.sh";

if (!-e $conf_file) {
    print "\nYou need to copy taransvar/misc/net_setup.conf to /root/taransvar and apply your personal setup before running this script.\n\nsudo cp net_setup.conf /root/taransvar\nsudo nano /root/taransvar/net_setup.conf\n";
    exit;
}

my %cfg = read_config($conf_file);

validate_config(\%cfg);

my $script = build_iptables_script(\%cfg);

open(my $out, ">", $out_file) or die "Cannot write $out_file: $!\n";
print $out $script;
close($out);

chmod 0755, $out_file or warn "Could not chmod +x $out_file: $!\n";

print "Generated $out_file\n";

sub read_config {
    my ($file) = @_;
    my %c;

    open(my $fh, "<", $file) or die "Cannot open $file: $!\n";
    while (my $line = <$fh>) {
        chomp $line;
        $line =~ s/\r$//;
        $line =~ s/^\s+|\s+$//g;
        next if $line eq '';
        next if $line =~ /^\s*#/;

        my ($k, $v) = split(/\s*=\s*/, $line, 2);
        next unless defined $k && defined $v;

        $k =~ s/^\s+|\s+$//g;
        $v =~ s/^\s+|\s+$//g;

        $c{$k} = $v;
    }
    close($fh);

    return %c;
}

sub validate_config {
    my ($c) = @_;

    for my $required (qw(
        NAME
        LAN_IF
        WAN_IF
        LAN_NET
        LAN_IP
        WAN_NET
        WAN_GW
        ALLOW_SSH
        SSH_PORT
        ALLOW_DHCP
        ALLOW_DNS
        ENABLE_NAT
        ENABLE_FORWARD_LAN_TO_WAN
        LOG_DROPS
        EXTRA_FORWARD_SRC
        EXTRA_FORWARD_IF
    )) {
        die "Missing required config key: $required\n" unless exists $c->{$required};
    }
}

sub build_iptables_script {
    my ($c) = @_;

    my $name                    = shell_quote($c->{NAME});
    my $lan_if                  = shell_quote($c->{LAN_IF});
    my $wan_if                  = shell_quote($c->{WAN_IF});
    my $lan_net                 = shell_quote($c->{LAN_NET});
    my $lan_ip                  = shell_quote($c->{LAN_IP});
    my $wan_net                 = shell_quote($c->{WAN_NET});
    my $wan_gw                  = shell_quote($c->{WAN_GW});
    my $allow_ssh               = is_true($c->{ALLOW_SSH});
    my $ssh_port                = int($c->{SSH_PORT});
    my $allow_dhcp              = is_true($c->{ALLOW_DHCP});
    my $allow_dns               = is_true($c->{ALLOW_DNS});
    my $enable_nat              = is_true($c->{ENABLE_NAT});
    my $enable_fwd_lan_to_wan   = is_true($c->{ENABLE_FORWARD_LAN_TO_WAN});
    my $log_drops               = is_true($c->{LOG_DROPS});
    my $extra_forward_src       = shell_quote($c->{EXTRA_FORWARD_SRC});
    my $extra_forward_if        = shell_quote($c->{EXTRA_FORWARD_IF});

    my $s = <<"EOF";
#!/bin/bash
set -e

NAME=$name
LAN_IF=$lan_if
WAN_IF=$wan_if
LAN_NET=$lan_net
LAN_IP=$lan_ip
WAN_NET=$wan_net
WAN_GW=$wan_gw
SSH_PORT=$ssh_port
EXTRA_FORWARD_SRC=$extra_forward_src
EXTRA_FORWARD_IF=$extra_forward_if

echo "Applying iptables rules for \$NAME..."
echo "LAN_IF=\$LAN_IF  WAN_IF=\$WAN_IF"
echo "LAN_NET=\$LAN_NET  LAN_IP=\$LAN_IP"
echo "WAN_NET=\$WAN_NET  WAN_GW=\$WAN_GW"

# Enable IPv4 forwarding
sysctl -w net.ipv4.ip_forward=1

# Flush existing rules
iptables -F
iptables -X
iptables -t nat -F
iptables -t nat -X
iptables -t mangle -F
iptables -t mangle -X
iptables -t raw -F
iptables -t raw -X

# Default policies
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# -------------------------
# INPUT chain
# -------------------------

# Allow loopback
iptables -A INPUT -i lo -j ACCEPT

# Allow established/related
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# Allow all traffic from LAN subnet on LAN interface
iptables -A INPUT -i \$LAN_IF -s \$LAN_NET -j ACCEPT

EOF

    if ($allow_ssh) {
        $s .= <<"EOF";
# Allow SSH
iptables -A INPUT -p tcp --dport \$SSH_PORT -j ACCEPT

EOF
    }

    if ($allow_dhcp) {
        $s .= <<"EOF";
# Allow DHCP server/client traffic on LAN side
iptables -A INPUT -i \$LAN_IF -p udp --sport 68 --dport 67 -j ACCEPT
iptables -A INPUT -i \$LAN_IF -p udp --sport 67 --dport 68 -j ACCEPT

EOF
    }

    if ($allow_dns) {
        $s .= <<"EOF";
# Allow DNS requests to this box
iptables -A INPUT -p udp --dport 53 -j ACCEPT
iptables -A INPUT -p tcp --dport 53 -j ACCEPT

EOF
    }

    $s .= <<"EOF";
# Optional ping to this host
iptables -A INPUT -p icmp --icmp-type echo-request -j ACCEPT

EOF

    if ($enable_fwd_lan_to_wan) {
        $s .= <<"EOF";
# -------------------------
# FORWARD chain - LAN to WAN
# -------------------------

# Allow established/related forwarded traffic
iptables -A FORWARD -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# Allow LAN -> WAN forwarding
iptables -A FORWARD -i \$LAN_IF -s \$LAN_NET -o \$WAN_IF -j ACCEPT

EOF
    } else {
        $s .= <<"EOF";
# -------------------------
# FORWARD chain
# -------------------------

# Allow established/related forwarded traffic
iptables -A FORWARD -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

EOF
    }

    $s .= <<"EOF";
# Allow extra forwarding source via specific interface
iptables -A FORWARD -i \$EXTRA_FORWARD_IF -s \$EXTRA_FORWARD_SRC -j ACCEPT
iptables -A FORWARD -o \$EXTRA_FORWARD_IF -d \$EXTRA_FORWARD_SRC -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

EOF

    if ($enable_nat) {
        $s .= <<"EOF";
# -------------------------
# NAT
# -------------------------

# Masquerade LAN subnet out WAN
iptables -t nat -A POSTROUTING -s \$LAN_NET -o \$WAN_IF -j MASQUERADE

# Masquerade extra forwarded source out WAN
iptables -t nat -A POSTROUTING -s \$EXTRA_FORWARD_SRC -o \$WAN_IF -j MASQUERADE

EOF
    }

    if ($log_drops) {
        $s .= <<"EOF";
# -------------------------
# Logging before final drops
# -------------------------

iptables -A INPUT   -m limit --limit 5/second --limit-burst 20 -j LOG --log-prefix "IPTABLES_INPUT_DROP \$NAME: " --log-level 4
iptables -A FORWARD -m limit --limit 5/second --limit-burst 20 -j LOG --log-prefix "IPTABLES_FORWARD_DROP \$NAME: " --log-level 4

EOF
    }

    $s .= <<"EOF";
echo "iptables rules applied."
echo
echo "Current filter table:"
iptables -L -v -n
echo
echo "Current nat table:"
iptables -t nat -L -v -n
EOF

    return $s;
}

sub is_true {
    my ($v) = @_;
    return 0 unless defined $v;
    return $v =~ /^(1|yes|true|on)$/i ? 1 : 0;
}

sub shell_quote {
    my ($s) = @_;
    $s //= '';
    $s =~ s/'/'"'"'/g;
    return "'$s'";
}