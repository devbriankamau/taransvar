#!/usr/bin/perl
#net_setup.pl
use strict;
use warnings;
use FindBin;

my $conf_file = "/root/taransvar/net_setup.conf";
my $out_file  = "/root/taransvar/iptables.sh";

if (!-e $conf_file) {
    print "
You need to copy taransvar/misc/net_setup.conf to /root/taransvar and apply your personal setup before running this script.

";
    print "sudo cp net_setup.conf /root/taransvar
";
    print "sudo nano /root/taransvar/net_setup.conf

";
    print "For now, it's writing a bash file for setting up iptables, but NOT running it.
";
    print "To run it: sudo bash $out_file
";
    exit;
}


my %cfg = read_config($conf_file);

#print "DEBUG loaded config keys:\n";
#for my $k (sort keys %cfg) {
#    print "  [$k] = [$cfg{$k}]\n";
#}

validate_config(\%cfg);

my $script = build_iptables_script(\%cfg);

open(my $out, ">", $out_file) or die "Cannot write $out_file: $!";
print $out $script;
close($out);

chmod 0755, $out_file or warn "Could not chmod +x $out_file: $!";

print "Generated $out_file\nTo run it: sudo bash $out_file\n\n";

sub read_config {
    my ($file) = @_;
    my %c;

    open(my $fh, "<", $file) or die "Cannot open $file: $!";
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

    for my $key (grep { /^PORT_FORWARD\d+$/ } keys %{$c}) {
        parse_port_forward($c->{$key});
    }

    for my $key (grep { /^OPEN_INCOMING_PORT\d+$/ } keys %{$c}) {
        parse_open_incoming_port($c->{$key});
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

get_if_ip() {
    local ifname="\$1"
    ip -4 -o addr show dev "\$ifname" | awk '{print \$4}' | head -n1 | cut -d/ -f1
}

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

my @open_keys = sort {
    key_index($a, 'OPEN_INCOMING_PORT') <=> key_index($b, 'OPEN_INCOMING_PORT')
} grep { /^OPEN_INCOMING_PORT\d+$/ } keys %{$c};


    if (@open_keys) {
        $s .= "# -------------------------\n# Additional incoming ports\n# -------------------------\n\n";
        for my $key (@open_keys) {
            my $oip = parse_open_incoming_port($c->{$key});
            my $ifname = shell_quote($oip->{ifname});
            my $proto  = lc($oip->{proto});
            my @ports  = @{$oip->{ports}};

            $s .= "# $key = $c->{$key}\n";
            for my $port (@ports) {
                if ($port =~ /^(\d+)-(\d+)$/) {
                    my ($start, $end) = ($1, $2);
                    my $range = "$start:$end";
                    if ($proto eq 'tcp' || $proto eq 'both') {
                        $s .= "iptables -A INPUT -i $ifname -p tcp --dport $range -j ACCEPT\n";
                    }
                    if ($proto eq 'udp' || $proto eq 'both') {
                        $s .= "iptables -A INPUT -i $ifname -p udp --dport $range -j ACCEPT\n";
                    }
                } else {
                    my $p = int($port);
                    if ($proto eq 'tcp' || $proto eq 'both') {
                        $s .= "iptables -A INPUT -i $ifname -p tcp --dport $p -j ACCEPT\n";
                    }
                    if ($proto eq 'udp' || $proto eq 'both') {
                        $s .= "iptables -A INPUT -i $ifname -p udp --dport $p -j ACCEPT\n";
                    }
                }
            }
            $s .= "\n";
        }
    }

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

my @pf_keys = sort {
    key_index($a, 'PORT_FORWARD') <=> key_index($b, 'PORT_FORWARD')
} grep { /^PORT_FORWARD\d+$/ } keys %{$c};

    if (@pf_keys) {
        $s .= "# -------------------------\n# Port forwarding rules\n# -------------------------\n\n";
        for my $key (@pf_keys) {
            my $pf = parse_port_forward($c->{$key});
            my $in_if        = shell_quote($pf->{in_if});
            my $public_port  = int($pf->{public_port});
            my $out_if       = shell_quote($pf->{out_if});
            my $target_ip    = shell_quote($pf->{target_ip});
            my $target_port  = int($pf->{target_port});

            $s .= <<"EOF";
# $key = $c->{$key}
PF_IN_IF=$in_if
PF_PUBLIC_PORT=$public_port
PF_OUT_IF=$out_if
PF_TARGET_IP=$target_ip
PF_TARGET_PORT=$target_port
PF_SNAT_IP=\$(get_if_ip \$PF_OUT_IF)
if [ -z "\$PF_SNAT_IP" ]; then
    echo "Could not determine IPv4 address for interface \$PF_OUT_IF used by $key" >&2
    exit 1
fi

iptables -t nat -A PREROUTING -i \$PF_IN_IF -p tcp --dport \$PF_PUBLIC_PORT \
    -j DNAT --to-destination \$PF_TARGET_IP:\$PF_TARGET_PORT

iptables -A FORWARD -i \$PF_IN_IF -o \$PF_OUT_IF -p tcp -d \$PF_TARGET_IP --dport \$PF_TARGET_PORT \
    -m conntrack --ctstate NEW,ESTABLISHED,RELATED -j ACCEPT

iptables -A FORWARD -i \$PF_OUT_IF -o \$PF_IN_IF -p tcp -s \$PF_TARGET_IP --sport \$PF_TARGET_PORT \
    -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

iptables -t nat -A POSTROUTING -o \$PF_OUT_IF -p tcp -d \$PF_TARGET_IP --dport \$PF_TARGET_PORT \
    -j SNAT --to-source \$PF_SNAT_IP

EOF
        }
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

sub parse_port_forward {
    my ($value) = @_;

    my ($left, $out_if, $right) = split(/\s*,\s*/, $value, 3);
    die "Invalid PORT_FORWARD format: $value\n" unless defined $left && defined $out_if && defined $right;

    my ($in_if, $public_port) = split(/:/, $left, 2);
    my ($target_ip, $target_port) = split(/:/, $right, 2);

    die "Invalid PORT_FORWARD incoming side: $value\n"
        unless defined $in_if && defined $public_port && $in_if ne '' && $public_port =~ /^\d+$/;
    die "Invalid PORT_FORWARD target side: $value\n"
        unless defined $target_ip && defined $target_port && $target_ip ne '' && $target_port =~ /^\d+$/;
    die "Invalid PORT_FORWARD outgoing interface: $value\n"
        unless defined $out_if && $out_if ne '';

    return {
        in_if       => $in_if,
        public_port => $public_port,
        out_if      => $out_if,
        target_ip   => $target_ip,
        target_port => $target_port,
    };
}

sub parse_open_incoming_port {
    my ($value) = @_;

    my ($ifname, $proto, @ports) = split(/\s*,\s*/, $value);
    die "Invalid OPEN_INCOMING_PORT format: $value\n"
        unless defined $ifname && defined $proto && @ports;

    die "Invalid OPEN_INCOMING_PORT interface: $value\n"
        unless $ifname ne '';

    $proto = lc($proto);
    die "Invalid OPEN_INCOMING_PORT protocol '$proto' in: $value\n"
        unless $proto =~ /^(tcp|udp|both)$/;

    for my $port (@ports) {
        die "Invalid OPEN_INCOMING_PORT port '$port' in: $value\n"
            unless $port =~ /^\d+$/ || $port =~ /^\d+-\d+$/;
        if ($port =~ /^(\d+)-(\d+)$/) {
            die "Invalid OPEN_INCOMING_PORT range '$port' in: $value\n" if $1 > $2;
        }
    }

    return {
        ifname => $ifname,
        proto  => $proto,
        ports  => \@ports,
    };
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

sub key_index {
    my ($key, $prefix) = @_;
    return 0 unless defined $key && defined $prefix;

    my $re = qr/^\Q$prefix\E(\d+)$/;
    if ($key =~ $re) {
        return int($1);
    }

    warn "Could not extract numeric suffix from key [$key] for prefix [$prefix]\n";
    return 0;
}