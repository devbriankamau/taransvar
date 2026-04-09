#!/usr/bin/perl
use strict;
use warnings;
use File::Temp qw(tempfile);

# ============================================================
# Taransvar lab reconciler / installer
#
# Architecture:
#   - host + routervm + node2 + node3 are peers on 10.10.10.0/24
#   - host gets lab bridge IP 10.10.10.254/24
#   - routervm may additionally get a USB Wi-Fi dongle and run
#     its own hotspot subnet separately (e.g. 10.20.20.0/24)
#
# Behavior:
#   - safe to run repeatedly
#   - creates/updates bridge + libvirt network
#   - creates Ubuntu 24.04 VMs from official cloud image
#   - absorbs later-added USB Wi-Fi dongles / extra cable NICs
#   - detects later-removed dongles / NICs and offers cleanup
# ============================================================

my $LAB_BRIDGE      = 'labbr0';
my $LAB_NET_NAME    = 'taransvar-lab';
my $LAB_SUBNET      = '10.10.10.0/24';
my $LAB_GATEWAY     = '10.10.10.254';
my $LAB_PREFIX      = 24;
my $ROUTERVM_IP     = '10.10.10.10';
my $HONEYPOT_IP     = '10.10.10.11';
my $NODE2_IP        = '10.10.10.2';
my $NODE3_IP        = '10.10.10.3';
my @TARGET_VMS      = qw(routervm node2 node3 honeypotvm);

# -------------------------
# Generic helpers
# -------------------------

sub run_cmd {
    my ($cmd) = @_;
    my $out = `$cmd 2>&1`;
    chomp $out;
    return $out;
}

sub system_ok {
    my ($cmd) = @_;
    return system($cmd) == 0 ? 1 : 0;
}

sub fail {
    my ($msg) = @_;
    die "ERROR: $msg\n";
}

sub info  { print "[INFO] $_[0]\n"; }
sub warnm { print "[WARN] $_[0]\n"; }

sub prompt {
    my ($msg, $default) = @_;
    print $msg;
    print " [$default]" if defined $default && $default ne '';
    print ": ";
    my $ans = <STDIN>;
    defined $ans or return defined $default ? $default : '';
    chomp $ans;
    return ($ans eq '' && defined $default) ? $default : $ans;
}

sub yesno {
    my ($msg, $default_yes) = @_;
    my $hint = $default_yes ? '[Y/n]' : '[y/N]';
    print "$msg $hint: ";
    my $ans = <STDIN>;
    defined $ans or return $default_yes ? 1 : 0;
    chomp $ans;
    $ans = lc $ans;
    return $default_yes if $ans eq '';
    return 1 if $ans eq 'y' || $ans eq 'yes';
    return 0;
}

sub choose_from_list {
    my ($title, @items) = @_;
    return undef unless @items;
    print "\n$title\n";
    for my $i (0 .. $#items) {
        print "  " . ($i + 1) . ") $items[$i]\n";
    }
    my $n = prompt("Choose number", 1);
    return undef unless defined $n && $n =~ /^\d+$/ && $n >= 1 && $n <= @items;
    return $items[$n - 1];
}

sub require_root {
    fail("Please run as root (sudo perl script.pl)") if $> != 0;
}

sub require_tools {
    my @tools = qw(virsh ip nmcli lsusb awk grep sed basename);
    for my $t (@tools) {
        system_ok("which $t >/dev/null 2>&1")
            or fail("Required tool '$t' not found");
    }
}

# -------------------------
# VM helpers
# -------------------------

sub virsh_vm_list {
    my $out = run_cmd("virsh list --all --name");
    return grep { $_ ne '' } split /\n/, $out;
}

sub vm_exists {
    my ($vm) = @_;
    my %have = map { $_ => 1 } virsh_vm_list();
    return $have{$vm} ? 1 : 0;
}

sub vm_state {
    my ($vm) = @_;
    return '' unless vm_exists($vm);
    return lc(run_cmd("virsh domstate '$vm'"));
}

sub ensure_vm_stopped {
    my ($vm) = @_;
    return unless vm_exists($vm);

    my $state = vm_state($vm);
    return if $state =~ /shut off/;

    info("VM '$vm' is not shut off (state: $state). Attempting graceful shutdown...");
    system("virsh shutdown '$vm' >/dev/null 2>&1");

    for (1..20) {
        sleep 1;
        my $cur = vm_state($vm);
        return if $cur =~ /shut off/;
    }

    warnm("Graceful shutdown of '$vm' failed. Forcing power off...");
    system_ok("virsh destroy '$vm' >/dev/null 2>&1")
        or fail("Failed to stop VM '$vm'");
}

sub rename_vm {
    my ($old, $new) = @_;
    fail("New VM name cannot be empty") unless $new;
    fail("Target VM name '$new' already exists") if vm_exists($new);

    ensure_vm_stopped($old);

    system_ok("virsh domrename '$old' '$new'")
        or fail("Failed to rename '$old' to '$new'");
    info("Renamed VM '$old' -> '$new'");
}

sub delete_vm {
    my ($name) = @_;
    ensure_vm_stopped($name);

    if (system_ok("virsh undefine '$name' --nvram --remove-all-storage >/dev/null 2>&1")) {
        info("Deleted VM '$name' with storage/nvram");
        return;
    }
    if (system_ok("virsh undefine '$name' --remove-all-storage >/dev/null 2>&1")) {
        info("Deleted VM '$name' with storage");
        return;
    }
    if (system_ok("virsh undefine '$name' >/dev/null 2>&1")) {
        info("Undefined VM '$name'");
        return;
    }
    fail("Failed to delete/undefine VM '$name'");
}

sub handle_existing_vms {
    my @existing = grep { vm_exists($_) } @TARGET_VMS;
    if (!@existing) {
        info("No existing routervm/node2/node3 VMs found.");
        return;
    }

    print "\nExisting lab VMs detected:\n";
    print "  - $_\n" for @existing;

    print "\nChoose what to do:\n";
    print "  1) Keep them as-is\n";
    print "  2) Rename selected VMs\n";
    print "  3) Delete selected VMs\n";
    my $choice = prompt("Selection", 1);

    if ($choice eq '2') {
        for my $vm (@existing) {
            next unless yesno("Rename '$vm'?", 0);
            my $new = prompt("New name for $vm");
            next unless $new;
            rename_vm($vm, $new);
        }
    }
    elsif ($choice eq '3') {
        for my $vm (@existing) {
            next unless yesno("Delete '$vm'?", 0);
            delete_vm($vm);
        }
    }
    else {
        info("Keeping existing VMs.");
    }
}

sub list_vm_interfaces {
    my ($vm) = @_;
    return '' unless vm_exists($vm);
    return run_cmd("virsh domiflist '$vm' 2>/dev/null");
}

# -------------------------
# Host NIC helpers
# -------------------------

sub default_interface {
    return run_cmd("ip route show default | awk '/default/ {print \$5; exit}'");
}

sub interface_ipv4 {
    my ($ifname) = @_;
    return run_cmd("ip -4 -o addr show dev '$ifname' | awk '{print \$4}' | cut -d/ -f1 | head -n1");
}

sub interface_kind {
    my ($ifname) = @_;
    my $wireless = run_cmd("iw dev 2>/dev/null | awk '\$1==\"Interface\"{print \$2}' | grep -x '$ifname'");
    return $wireless ? 'wifi' : 'ethernet';
}

sub interface_path {
    my ($ifname) = @_;
    my $p = readlink("/sys/class/net/$ifname/device");
    return defined $p ? $p : '';
}

sub interface_driver {
    my ($ifname) = @_;
    return run_cmd("basename \$(readlink /sys/class/net/$ifname/device/driver 2>/dev/null)");
}

sub get_interfaces {
    my %ifs;
    my $out = run_cmd("ip -o link show");

    for my $line (split /\n/, $out) {
        next unless $line =~ /^\d+:\s+([^:]+):/;
        my $ifname = $1;

        next if $ifname eq 'lo';
        next if $ifname =~ /^virbr/;
        next if $ifname =~ /^vnet/;
        next if $ifname =~ /^docker/;
        next if $ifname =~ /^br-/;

        $ifs{$ifname}{name}    = $ifname;
        $ifs{$ifname}{kind}    = interface_kind($ifname);
        $ifs{$ifname}{ip}      = interface_ipv4($ifname);
        $ifs{$ifname}{driver}  = interface_driver($ifname);
        $ifs{$ifname}{path}    = interface_path($ifname);
        $ifs{$ifname}{state}   = run_cmd("cat /sys/class/net/$ifname/operstate");
        $ifs{$ifname}{carrier} = run_cmd("cat /sys/class/net/$ifname/carrier 2>/dev/null") eq '1' ? 1 : 0;
        $ifs{$ifname}{mac}     = run_cmd("cat /sys/class/net/$ifname/address");
    }

    return %ifs;
}

sub classify_interfaces {
    my (%ifs) = @_;

    my @all           = sort keys %ifs;
    my @wifi          = grep { $ifs{$_}{kind} eq 'wifi' } @all;
    my @eth           = grep { $ifs{$_}{kind} eq 'ethernet' } @all;
    my @usb_wifi      = grep { $ifs{$_}{kind} eq 'wifi'     && ($ifs{$_}{path} =~ /usb/i || $_ =~ /^wlx/) } @all;
    my @builtin_wifi  = grep { $ifs{$_}{kind} eq 'wifi'     && !($ifs{$_}{path} =~ /usb/i || $_ =~ /^wlx/) } @all;
    my @usb_eth       = grep { $ifs{$_}{kind} eq 'ethernet' && $ifs{$_}{path} =~ /usb/i } @all;
    my @builtin_eth   = grep { $ifs{$_}{kind} eq 'ethernet' && !($ifs{$_}{path} =~ /usb/i) } @all;

    return {
        all           => \@all,
        wifi          => \@wifi,
        eth           => \@eth,
        usb_wifi      => \@usb_wifi,
        builtin_wifi  => \@builtin_wifi,
        usb_eth       => \@usb_eth,
        builtin_eth   => \@builtin_eth,
    };
}

sub print_interfaces {
    my (%ifs) = @_;
    print "\nDetected host NICs:\n";
    for my $if (sort keys %ifs) {
        printf "  %-18s kind=%-8s state=%-8s carrier=%s ip=%-15s driver=%s\n",
            $if,
            $ifs{$if}{kind} || '?',
            $ifs{$if}{state} || '?',
            $ifs{$if}{carrier} ? '1' : '0',
            $ifs{$if}{ip} || '-',
            $ifs{$if}{driver} || '-';
    }
}

sub interface_is_default_uplink {
    my ($ifname) = @_;
    return default_interface() eq $ifname ? 1 : 0;
}

# -------------------------
# USB Wi-Fi detection
# -------------------------

sub detect_usb_wifi_dongles {
    my @found;

    my $link_out = run_cmd("ip -o link show");
    for my $line (split /\n/, $link_out) {
        next unless $line =~ /^\d+:\s+([^:]+):/;
        my $ifname = $1;
        next if $ifname eq 'lo';

        my $is_wifi = run_cmd("iw dev 2>/dev/null | awk '\$1==\"Interface\"{print \$2}' | grep -x '$ifname'");
        next unless $is_wifi;

        my $path = readlink("/sys/class/net/$ifname/device");
        $path = defined $path ? $path : '';

        if ($path =~ /usb/i || $ifname =~ /^wlx/i) {
            push @found, {
                type   => 'host_interface',
                ifname => $ifname,
                path   => $path,
                desc   => "USB Wi-Fi interface $ifname",
            };
        }
    }

    my $lsusb = run_cmd("lsusb");
    for my $line (split /\n/, $lsusb) {
        next unless $line =~ /^Bus\s+(\d+)\s+Device\s+(\d+):\s+ID\s+([0-9a-f]{4}):([0-9a-f]{4})\s+(.+)$/i;
        my ($bus, $dev, $vendor, $product, $desc) = ($1, $2, lc($3), lc($4), $5);

        if (
            $desc =~ /(wireless|wifi|wi-fi|802\.11|wlan|wireless_device|mediatek|ralink|rtl88|rtl8u|rtl881|rtl882|rtl885|ath9|ath10|ath11|mt76|imc)/i
            && $desc !~ /(ethernet|gigabit|lan|hub)/i
        ) {
            push @found, {
                type    => 'usb_device',
                bus     => $bus,
                device  => $dev,
                vendor  => $vendor,
                product => $product,
                desc    => $desc,
            };
        }
    }

    if (vm_exists('routervm')) {
        my $xml = run_cmd("virsh dumpxml routervm");
        while ($xml =~ m{
            <hostdev\s+mode='subsystem'\s+type='usb'.*?
            <vendor\s+id='0x([0-9a-f]{4})'\s*/>.*?
            <product\s+id='0x([0-9a-f]{4})'\s*/>
        }sgix) {
            push @found, {
                type    => 'attached_to_routervm',
                vendor  => lc($1),
                product => lc($2),
                desc    => "Already attached to routervm",
            };
        }
    }

    my %seen;
    my @uniq;
    for my $d (@found) {
        my $key =
            $d->{type} eq 'host_interface'        ? "hostif:$d->{ifname}" :
            $d->{type} eq 'usb_device'            ? "usb:$d->{vendor}:$d->{product}" :
            $d->{type} eq 'attached_to_routervm'  ? "vm:$d->{vendor}:$d->{product}" :
                                                    "other";
        next if $seen{$key}++;
        push @uniq, $d;
    }

    return @uniq;
}

sub usb_device_present_on_host {
    my ($vendor, $product) = @_;
    my $lsusb = run_cmd("lsusb");
    return $lsusb =~ /\b\Q$vendor\E:\Q$product\E\b/i ? 1 : 0;
}

sub get_routervm_usb_hostdevs {
    my @devs;
    return @devs unless vm_exists('routervm');

    my $xml = run_cmd("virsh dumpxml routervm");
    while ($xml =~ m{
        <hostdev\s+mode='subsystem'\s+type='usb'.*?
        <vendor\s+id='0x([0-9a-f]{4})'\s*/>.*?
        <product\s+id='0x([0-9a-f]{4})'\s*/>
    }sgix) {
        push @devs, {
            vendor  => lc($1),
            product => lc($2),
        };
    }
    return @devs;
}

# -------------------------
# Plan detection
# -------------------------

sub decide_topology {
    my (%ifs) = @_;
    my $class = classify_interfaces(%ifs);
    my $default_if = default_interface();
    my $internet_kind = $default_if ? ($ifs{$default_if}{kind} || 'unknown') : 'unknown';

    my %plan = (
        uplink_if                => $default_if,
        uplink_kind              => $internet_kind,
        built_in_only            => 0,
        use_extra_lan            => '',
        use_wifi_dongle          => 0,
        use_builtin_lan_for_lab  => '',
        notes                    => [],
    );

    my @usb_eth      = @{$class->{usb_eth}};
    my @builtin_eth  = @{$class->{builtin_eth}};
    my @builtin_wifi = @{$class->{builtin_wifi}};

    my @dongles = detect_usb_wifi_dongles();
    my $has_any_wifi_dongle = scalar grep {
        $_->{type} eq 'usb_device' || $_->{type} eq 'host_interface' || $_->{type} eq 'attached_to_routervm'
    } @dongles;

    if (@builtin_eth && @builtin_wifi && !@usb_eth) {
        $plan{built_in_only} = 1;
        push @{$plan{notes}}, "Host appears to be a laptop with built-in LAN + Wi-Fi.";

        if ($internet_kind eq 'wifi' && @builtin_eth) {
            $plan{use_builtin_lan_for_lab} = $builtin_eth[0];
            push @{$plan{notes}}, "Internet uses Wi-Fi '$default_if'; built-in LAN '$builtin_eth[0]' is available for cable lab access.";
        }
        elsif ($internet_kind eq 'ethernet') {
            push @{$plan{notes}}, "Internet uses Ethernet '$default_if'; unless another uplink appears, built-in cable lab expansion is limited.";
        }
    }

    if (@usb_eth) {
        $plan{use_extra_lan} = $usb_eth[0];
        push @{$plan{notes}}, "Extra USB Ethernet NIC '$usb_eth[0]' detected for lab cable access.";
    }

    if ($has_any_wifi_dongle) {
        $plan{use_wifi_dongle} = 1;
        push @{$plan{notes}}, "USB Wi-Fi dongle detected or already attached to routervm.";
    }

    if (!$plan{use_extra_lan} && !$plan{use_builtin_lan_for_lab}) {
        push @{$plan{notes}}, "No clear extra lab cable NIC found yet; the script will still create the virtual lab bridge now.";
    }

    return \%plan;
}

sub print_plan_summary {
    my ($plan) = @_;

    print "\n=== Intended lab layout ===\n";
    print "Lab bridge/network : $LAB_BRIDGE / $LAB_NET_NAME\n";
    print "Lab subnet         : $LAB_SUBNET\n";
    print "Host bridge IP     : $LAB_GATEWAY/$LAB_PREFIX\n";
    print "routervm           : $ROUTERVM_IP\n";
    print "honeypotvm         : $HONEYPOT_IP\n";
    print "node2              : $NODE2_IP\n";
    print "node3              : $NODE3_IP\n";
    print "routervm role      : ordinary node on 10.10.10.0/24, plus optional hotspot subnet via USB Wi-Fi dongle\n";
    print "node2/node3 role   : ordinary nodes on 10.10.10.0/24\n";

    if ($plan->{use_extra_lan}) {
        print "Extra cable NIC    : $plan->{use_extra_lan} -> can extend 10.10.10.0/24 to physical cabled devices\n";
    }
    elsif ($plan->{use_builtin_lan_for_lab}) {
        print "Built-in LAN for lab: $plan->{use_builtin_lan_for_lab}\n";
    }
    else {
        print "Cable lab NIC      : none clearly available yet\n";
    }

    print "Wi-Fi dongle       : " . ($plan->{use_wifi_dongle} ? "available / already attached" : "not currently detected") . "\n";

    print "\nNotes:\n";
    print "  - $_\n" for @{$plan->{notes}};
    print "\n";
}

# -------------------------
# Downloads / Ubuntu image / cloud-init helpers
# -------------------------

sub downloads_dir {
    my $user = $ENV{SUDO_USER} || $ENV{USER} || 'root';
    my $home = $user eq 'root' ? '/root' : (getpwnam($user))[7];
    $home ||= $ENV{HOME} || '/root';
    return "$home/Downloads";
}

sub ensure_dir {
    my ($dir) = @_;
    return if -d $dir;
    mkdir $dir or fail("Failed to create directory '$dir': $!");
}

sub ensure_cloud_tools {
    my @pkgs = qw(virt-install qemu-utils cloud-image-utils wget openssl genisoimage);
    for my $bin (qw(virt-install qemu-img wget cloud-localds openssl)) {
        next if system_ok("which $bin >/dev/null 2>&1");
        warnm("Missing tool '$bin'. Installing required packages...");
        system_ok("apt-get update") or fail("apt-get update failed");
        system_ok("apt-get install -y @pkgs")
            or fail("Failed to install required packages: @pkgs");
        last;
    }
}

sub ubuntu_cloud_image_download_path {
    my $dl = downloads_dir();
    ensure_dir($dl);
    return "$dl/ubuntu-24.04-server-cloudimg-amd64.img";
}

sub ubuntu_cloud_image_pool_path {
    return "/var/lib/libvirt/images/ubuntu-24.04-server-cloudimg-amd64.img";
}

sub ensure_ubuntu_cloud_image {
    my $download_img = ubuntu_cloud_image_download_path();
    my $pool_img     = ubuntu_cloud_image_pool_path();

    if (!-f $download_img) {
        my $url = "https://cloud-images.ubuntu.com/releases/noble/release/ubuntu-24.04-server-cloudimg-amd64.img";
        info("Downloading Ubuntu 24.04 cloud image to $download_img");
        system_ok("wget -O '$download_img' '$url'")
            or fail("Failed to download Ubuntu cloud image from $url");
    } else {
        info("Ubuntu cloud image already present in Downloads: $download_img");
    }

    if (!-f $pool_img) {
        info("Copying Ubuntu cloud image into libvirt storage: $pool_img");
        system_ok("cp '$download_img' '$pool_img'")
            or fail("Failed to copy cloud image into libvirt storage pool");
        system_ok("chmod 644 '$pool_img'")
            or fail("Failed to chmod '$pool_img'");
    } else {
        info("Ubuntu cloud image already present in libvirt storage: $pool_img");
    }

    return $pool_img;
}

sub vm_disk_path {
    my ($name) = @_;
    return "/var/lib/libvirt/images/${name}.qcow2";
}

sub seed_iso_path {
    my ($name) = @_;
    return "/var/lib/libvirt/images/${name}-seed.iso";
}

sub create_vm_disk_from_cloud_image {
    my ($name, $size_gb) = @_;
    my $base = ensure_ubuntu_cloud_image();
    my $disk = vm_disk_path($name);

    return $disk if -f $disk;

    system_ok("qemu-img create -f qcow2 -F qcow2 -b '$base' '$disk' ${size_gb}G")
        or fail("Failed to create VM disk '$disk'");
    info("Created disk '$disk' from Ubuntu 24.04 cloud image");
    return $disk;
}

sub cloud_init_password_hash {
    my $plain = shift;
    my $escaped = $plain;
    $escaped =~ s/'/'"'"'/g;
    my $hash = run_cmd("openssl passwd -6 '$escaped'");
    fail("Failed to generate password hash") unless $hash;
    return $hash;
}

sub create_cloud_init_seed {
    my (%args) = @_;
    my $name      = $args{name};
    my $ip        = $args{ip};
    my $prefix    = $args{prefix} || 24;
    my $gateway   = $args{gateway} || '';
    my $dns       = $args{dns} || '1.1.1.1,8.8.8.8';
    my $user      = $args{user} || 'ubuntu';
    my $password  = $args{password} || 'ubuntu';
    my $mac       = lc($args{mac} || '');
    my $seed_iso  = seed_iso_path($name);

    my $hash = cloud_init_password_hash($password);

    my ($ufh, $user_data) = tempfile(SUFFIX => ".user-data");
print $ufh <<"EOF";
#cloud-config
hostname: $name
manage_etc_hosts: true

users:
  - name: $user
    sudo: ALL=(ALL) NOPASSWD:ALL
    groups: sudo
    shell: /bin/bash
    lock_passwd: false

chpasswd:
  list: |
    $user:$password
  expire: false

ssh_pwauth: true
disable_root: false
package_update: true
packages:
  - qemu-guest-agent
  - net-tools
  - curl
  - wget
EOF
    close $ufh;
    my ($mfh, $meta_data) = tempfile(SUFFIX => ".meta-data");
    print $mfh <<"EOF";
instance-id: $name
local-hostname: $name
EOF
    close $mfh;

    my ($nfh, $network_cfg) = tempfile(SUFFIX => ".network-config");
    print $nfh <<"EOF";
version: 2
ethernets:
  default:
    match:
      name: "en*"
    dhcp4: false
    addresses:
      - $ip/$prefix
    routes:
      - to: default
        via: $gateway
    nameservers:
      addresses: [$dns]
EOF
    close $nfh;

    system_ok("cloud-localds --network-config='$network_cfg' '$seed_iso' '$user_data' '$meta_data'")
        or fail("Failed to create cloud-init seed ISO '$seed_iso'");

    info("Created cloud-init seed '$seed_iso' for VM '$name'");
    return $seed_iso;
}

sub ensure_storage_pool_default {
    system_ok("virsh pool-info default >/dev/null 2>&1") or do {
        my $dir = "/var/lib/libvirt/images";
        mkdir $dir unless -d $dir;
        system_ok("virsh pool-define-as default dir - - - - '$dir'")
            or fail("Failed to define default storage pool");
        system_ok("virsh pool-build default")     or warnm("Could not build default pool");
        system_ok("virsh pool-start default")     or warnm("Could not start default pool");
        system_ok("virsh pool-autostart default") or warnm("Could not autostart default pool");
    };
}

sub vm_existing_mac {
    my ($name) = @_;
    return '' unless vm_exists($name);

    my $xml = run_cmd("virsh dumpxml '$name'");
    if ($xml =~ /<mac address=['"]([^'"]+)['"]/) {
        return lc($1);
    }
    return '';
}

sub mac_in_use {
    my ($mac) = @_;
    my $out = run_cmd("virsh list --all --name");
    for my $vm (grep { $_ ne '' } split /\n/, $out) {
        my $xml = run_cmd("virsh dumpxml '$vm'");
        return 1 if $xml =~ /\Q$mac\E/i;
    }
    return 0;
}


sub vm_mac_for_name {
    my ($name) = @_;

    my $existing = vm_existing_mac($name);
    return $existing if $existing;

    my %preferred = (
        routervm => '52:54:00:10:10:01',
        node2    => '52:54:00:10:10:02',
        node3    => '52:54:00:10:10:03',
    );

    if (exists $preferred{$name} && !mac_in_use($preferred{$name})) {
        return $preferred{$name};
    }

    for my $i (4 .. 254) {
        my $mac = sprintf("52:54:00:10:10:%02x", $i);
        return $mac unless mac_in_use($mac);
    }

    fail("No free MAC address available in planned range");
}

sub define_or_install_cloud_vm {
    my (%args) = @_;
    my $name     = $args{name};
    my $memory   = $args{memory} || 3072;
    my $vcpus    = $args{vcpus}  || 2;
    my $disk_gb  = $args{disk_gb} || 12;
    my $ip       = $args{ip};
    my $gateway  = $args{gateway} || '';
    my $user     = $args{user} || 'ubuntu';
    my $password = $args{password} || 'ubuntu';
    my $mac = vm_mac_for_name($name);

    return if vm_exists($name);

    my $disk = create_vm_disk_from_cloud_image($name, $disk_gb);
    my $seed = create_cloud_init_seed(
        name     => $name,
        ip       => $ip,
        prefix   => 24,
        gateway  => $gateway,
        user     => $user,
        password => $password,
        mac      => $mac,
    );

    my $cmd = join " ",
        "virt-install",
        "--name '$name'",
        "--memory $memory",
        "--vcpus $vcpus",
        "--import",
        "--os-variant ubuntu24.04",
        "--disk path='$disk',format=qcow2,bus=virtio",
        "--disk path='$seed',device=cdrom",
        "--network network='$LAB_NET_NAME',model=virtio,mac='$mac'",        
        "--graphics vnc,listen=127.0.0.1",
        "--console pty,target_type=serial",
        "--noautoconsole";

    system_ok($cmd) or fail("Failed to create/install VM '$name'");
    info("Created Ubuntu 24.04 VM '$name'");
}

sub next_free_node_ip {
    my %reserved = map { $_ => 1 } ($ROUTERVM_IP, $HONEYPOT_IP, $NODE2_IP, $NODE3_IP, $LAB_GATEWAY);

    for my $last (4 .. 253) {
        my $candidate = "10.10.10.$last";
        next if $reserved{$candidate};
        return $candidate;
    }

    fail("No free IP left in 10.10.10.0/24");
}

sub maybe_create_extra_node {
    return unless yesno("Add another node to the 10.10.10.0/24 network?", 0);

    my $name = prompt("Name for the new VM");
    fail("VM name cannot be empty") unless $name;
    fail("VM '$name' already exists") if vm_exists($name);

    my $ip   = prompt("IP for $name", next_free_node_ip());
    my $ram  = prompt("RAM in MiB for $name", 3072);
    my $cpu  = prompt("vCPUs for $name", 2);
    my $disk = prompt("Disk size in GB for $name", 12);
    my $pw   = prompt("Password for ubuntu user on $name", "ubuntu");

    define_or_install_cloud_vm(
        name     => $name,
        memory   => $ram,
        vcpus    => $cpu,
        disk_gb  => $disk,
        ip       => $ip,
        gateway  => '',
        user     => 'ubuntu',
        password => $pw,
    );
}

# -------------------------
# Bridge / NetworkManager helpers
# -------------------------

sub nmcli_connection_exists {
    my ($name) = @_;
    my $out = run_cmd("nmcli -t -f NAME connection show");
    my %have = map { $_ => 1 } grep { $_ ne '' } split /\n/, $out;
    return $have{$name} ? 1 : 0;
}

sub interface_is_bridge_slave {
    my ($ifname, $bridge) = @_;
    my $master = run_cmd("basename \$(readlink /sys/class/net/$ifname/master 2>/dev/null)");
    return ($master && $master eq $bridge) ? 1 : 0;
}

sub bridge_exists {
    my ($bridge) = @_;
    return -d "/sys/class/net/$bridge" ? 1 : 0;
}

sub create_bridge_nmcli {
    my ($bridge, $ip_cidr, $slave_if) = @_;

    if (!nmcli_connection_exists($bridge)) {
        system_ok("nmcli connection add type bridge ifname '$bridge' con-name '$bridge'")
            or fail("Failed to create bridge '$bridge'");
        info("Created bridge connection '$bridge'");
    } else {
        info("Bridge connection '$bridge' already exists");
    }

    system_ok("nmcli connection modify '$bridge' ipv4.addresses '$ip_cidr' ipv4.method manual ipv6.method ignore")
        or fail("Failed to configure bridge '$bridge'");

    if ($slave_if) {
        if (interface_is_default_uplink($slave_if)) {
            warnm("Refusing to automatically enslave '$slave_if' because it is the current default uplink.");
        }
        elsif (interface_is_bridge_slave($slave_if, $bridge)) {
            info("Interface '$slave_if' is already enslaved into '$bridge'");
        } else {
            my $slave_con = "${bridge}-slave-$slave_if";
            if (!nmcli_connection_exists($slave_con)) {
                system_ok("nmcli connection add type bridge-slave ifname '$slave_if' master '$bridge' con-name '$slave_con'")
                    or fail("Failed to add '$slave_if' as bridge slave");
                info("Added '$slave_if' as slave of '$bridge'");
            } else {
                info("Bridge-slave connection '$slave_con' already exists");
            }

            system_ok("nmcli connection up '$slave_con' >/dev/null 2>&1")
                or warnm("Could not bring slave '$slave_con' up immediately");
        }
    }

    system_ok("nmcli connection up '$bridge' >/dev/null 2>&1")
        or warnm("Could not bring bridge '$bridge' up immediately");
}

sub nmcli_bridge_slaves {
    my ($bridge) = @_;
    my @slaves;

    my $names = run_cmd("nmcli -t -f NAME connection show");
    for my $name (grep { $_ ne '' } split /\n/, $names) {
        my $master = run_cmd("nmcli -g connection.master connection show '$name' 2>/dev/null");
        next unless $master && $master eq $bridge;

        my $ctype  = run_cmd("nmcli -g connection.type connection show '$name' 2>/dev/null");
        my $device = run_cmd("nmcli -g GENERAL.DEVICES connection show '$name' 2>/dev/null");

        push @slaves, {
            con_name => $name,
            type     => $ctype,
            device   => $device,
            master   => $master,
        };
    }

    return @slaves;
}

sub remove_nmcli_connection {
    my ($name) = @_;
    system_ok("nmcli connection delete '$name'")
        or warnm("Failed to delete connection '$name'");
}

# -------------------------
# Libvirt network helpers
# -------------------------

sub libvirt_net_exists {
    my ($name) = @_;
    my $out = run_cmd("virsh net-list --all --name");
    my %have = map { $_ => 1 } grep { $_ ne '' } split /\n/, $out;
    return $have{$name} ? 1 : 0;
}

sub create_libvirt_bridge_network {
    my ($name, $bridge) = @_;

    my ($fh, $xmlfile) = tempfile(SUFFIX => ".xml");
    print $fh <<"XML";
<network>
  <name>$name</name>
  <forward mode='bridge'/>
  <bridge name='$bridge'/>
</network>
XML
    close $fh;

    if (!libvirt_net_exists($name)) {
        system_ok("virsh net-define '$xmlfile'")
            or fail("Failed to define libvirt network '$name'");
        info("Defined libvirt network '$name'");
    } else {
        info("Libvirt network '$name' already exists");
    }

    system_ok("virsh net-autostart '$name'")
        or fail("Failed to set autostart for libvirt network '$name'");

    system_ok("virsh net-start '$name' >/dev/null 2>&1")
        or info("Libvirt network '$name' already active or could not be started immediately");
}

# -------------------------
# Network attach helpers
# -------------------------

sub attach_network_to_vm {
    my ($vm, $netname) = @_;

    system_ok("virsh attach-interface '$vm' network '$netname' --model virtio --config")
        or fail("Failed to attach network '$netname' to '$vm'");

    if (vm_state($vm) =~ /running/) {
        system_ok("virsh attach-interface '$vm' network '$netname' --model virtio --live >/dev/null 2>&1")
            or warnm("Could not live-attach network '$netname' to '$vm'; config attach is saved");
    }

    info("Attached network '$netname' to '$vm'");
}

# -------------------------
# USB attach/detach helpers
# -------------------------

sub attach_usb_hostdev_to_vm {
    my ($vm, $vendor, $product) = @_;

    my ($fh, $xmlfile) = tempfile(SUFFIX => ".xml");
    print $fh <<"XML";
<hostdev mode='subsystem' type='usb' managed='yes'>
  <source>
    <vendor id='0x$vendor'/>
    <product id='0x$product'/>
  </source>
</hostdev>
XML
    close $fh;

    if (system_ok("virsh attach-device '$vm' '$xmlfile' --config")) {
        info("Attached USB device ${vendor}:${product} to '$vm' (config)");
    } else {
        fail("Failed to attach USB device ${vendor}:${product} to '$vm'");
    }

    if (vm_state($vm) =~ /running/) {
        system_ok("virsh attach-device '$vm' '$xmlfile' --live >/dev/null 2>&1")
            or warnm("Could not live-attach USB device to running '$vm'; config attach is saved");
    }
}

sub detach_usb_hostdev_from_vm {
    my ($vm, $vendor, $product) = @_;

    my ($fh, $xmlfile) = tempfile(SUFFIX => ".xml");
    print $fh <<"XML";
<hostdev mode='subsystem' type='usb' managed='yes'>
  <source>
    <vendor id='0x$vendor'/>
    <product id='0x$product'/>
  </source>
</hostdev>
XML
    close $fh;

    system_ok("virsh detach-device '$vm' '$xmlfile' --config")
        or warnm("Could not detach USB ${vendor}:${product} from '$vm' config");

    if (vm_state($vm) =~ /running/) {
        system_ok("virsh detach-device '$vm' '$xmlfile' --live >/dev/null 2>&1")
            or warnm("Could not live-detach USB ${vendor}:${product} from '$vm'");
    }

    info("Detached USB ${vendor}:${product} from '$vm'");
}

# -------------------------
# Main reconcilers
# -------------------------

sub maybe_prepare_bridge_for_lab {
    my ($plan, %ifs) = @_;

    my $chosen_if = '';
    if ($plan->{use_extra_lan}) {
        $chosen_if = $plan->{use_extra_lan};
    }
    elsif ($plan->{use_builtin_lan_for_lab}) {
        $chosen_if = $plan->{use_builtin_lan_for_lab};
    }

    info("Preparing virtual lab bridge '$LAB_BRIDGE' for 10.10.10.0/24.");

    if ($chosen_if) {
        if (interface_is_default_uplink($chosen_if)) {
            warnm("Chosen lab NIC '$chosen_if' is the current default uplink. The bridge will be created without enslaving it.");
            create_bridge_nmcli($LAB_BRIDGE, "$LAB_GATEWAY/$LAB_PREFIX", '');
        } else {
            create_bridge_nmcli($LAB_BRIDGE, "$LAB_GATEWAY/$LAB_PREFIX", $chosen_if);
        }
    } else {
        info("No clear lab cable NIC found. Creating bridge '$LAB_BRIDGE' without a physical slave.");
        create_bridge_nmcli($LAB_BRIDGE, "$LAB_GATEWAY/$LAB_PREFIX", '');
    }

    create_libvirt_bridge_network($LAB_NET_NAME, $LAB_BRIDGE);
}

sub maybe_create_vms {
    ensure_storage_pool_default();
    ensure_cloud_tools();

    my $default_pw = prompt("Default password for ubuntu user on lab VMs", "ubuntu");

    my @vm_specs = (
        {
            name     => 'routervm',
            memory   => 4096,
            vcpus    => 2,
            disk_gb  => 20,
            ip       => $ROUTERVM_IP,
        },
        {
            name     => 'honeypotvm',
            memory   => 2048,
            vcpus    => 2,
            disk_gb  => 12,
            ip       => $HONEYPOT_IP,
        },        
        {
            name     => 'node2',
            memory   => 3072,
            vcpus    => 2,
            disk_gb  => 12,
            ip       => $NODE2_IP,
        },
        {
            name     => 'node3',
            memory   => 3072,
            vcpus    => 2,
            disk_gb  => 12,
            ip       => $NODE3_IP,
        },
    );

    for my $vm (@vm_specs) {
        next if vm_exists($vm->{name});

        next unless yesno("Create and install Ubuntu 24.04 VM '$vm->{name}'?", 1);

        define_or_install_cloud_vm(
            name     => $vm->{name},
            memory   => $vm->{memory},
            vcpus    => $vm->{vcpus},
            disk_gb  => $vm->{disk_gb},
            ip       => $vm->{ip},
            gateway  => '',
            user     => 'ubuntu',
            password => $default_pw,
        );
    }

    maybe_create_extra_node();
}

sub maybe_attach_networks_to_existing_vms {
    for my $vm (@TARGET_VMS) {
        next unless vm_exists($vm);
        my $iflist = list_vm_interfaces($vm);

        if ($iflist !~ /\Q$LAB_NET_NAME\E/ && $iflist !~ /\Q$LAB_BRIDGE\E/) {
            next unless yesno("Attach lab network '$LAB_NET_NAME' to '$vm'?", 1);
            attach_network_to_vm($vm, $LAB_NET_NAME);
        } else {
            info("VM '$vm' already appears to be attached to the lab network.");
        }
    }
}

sub maybe_attach_dongle_to_routervm {
    return unless vm_exists('routervm');

    my @candidates = detect_usb_wifi_dongles();
    if (!@candidates) {
        info("No USB Wi-Fi dongle candidates detected.");
        return;
    }

    my %already;
    for my $dev (@candidates) {
        if ($dev->{type} eq 'attached_to_routervm') {
            my $key = "$dev->{vendor}:$dev->{product}";
            $already{$key} = 1;
            info("USB dongle $key is already attached to routervm.");
        }
    }

    for my $dev (@candidates) {
        next if $dev->{type} eq 'attached_to_routervm';

        if ($dev->{type} eq 'host_interface') {
            info("Detected USB Wi-Fi host interface: $dev->{ifname}");
            next;
        }

        next unless $dev->{type} eq 'usb_device';
        my $key = "$dev->{vendor}:$dev->{product}";
        next if $already{$key};

        print "\nUSB Wi-Fi dongle candidate:\n";
        print "  ID   : $dev->{vendor}:$dev->{product}\n";
        print "  Desc : $dev->{desc}\n";

        if (yesno("Attach this dongle to routervm (if not already attached)?", 1)) {
            attach_usb_hostdev_to_vm('routervm', $dev->{vendor}, $dev->{product});
            $already{$key} = 1;
        }
    }
}

sub maybe_add_new_cable_nic_to_lab {
    my (%ifs) = @_;
    return unless bridge_exists($LAB_BRIDGE);

    my $class = classify_interfaces(%ifs);

    my @candidates = (
        @{$class->{usb_eth}},
        grep { !interface_is_default_uplink($_) } @{$class->{builtin_eth}}
    );

    my %seen;
    @candidates = grep { !$seen{$_}++ } @candidates;

    for my $if (@candidates) {
        next if $if eq $LAB_BRIDGE;
        next if interface_is_bridge_slave($if, $LAB_BRIDGE);

        print "\nNew cable NIC candidate detected for lab: $if\n";
        print "  kind   : $ifs{$if}{kind}\n";
        print "  state  : $ifs{$if}{state}\n";
        print "  driver : $ifs{$if}{driver}\n";

        if (yesno("Add '$if' into bridge '$LAB_BRIDGE' for direct 10.10.10.0/24 access?", 1)) {
            create_bridge_nmcli($LAB_BRIDGE, "$LAB_GATEWAY/$LAB_PREFIX", $if);
        }
    }
}

sub reconcile_missing_routervm_dongles {
    return unless vm_exists('routervm');

    my @attached = get_routervm_usb_hostdevs();
    for my $d (@attached) {
        next if usb_device_present_on_host($d->{vendor}, $d->{product});

        warnm("routervm has USB device $d->{vendor}:$d->{product} configured, but it is not present on the host.");

        if (yesno("Remove stale USB attachment $d->{vendor}:$d->{product} from routervm config?", 1)) {
            detach_usb_hostdev_from_vm('routervm', $d->{vendor}, $d->{product});
        }
    }
}

sub reconcile_missing_bridge_slaves {
    return unless bridge_exists($LAB_BRIDGE);

    my @slaves = nmcli_bridge_slaves($LAB_BRIDGE);
    for my $s (@slaves) {
        my $dev = $s->{device};
        my $missing = (!$dev || $dev eq '--' || !-e "/sys/class/net/$dev") ? 1 : 0;
        next unless $missing;

        warnm("Bridge slave connection '$s->{con_name}' refers to a missing NIC '$dev'.");

        if (yesno("Remove stale bridge-slave connection '$s->{con_name}'?", 1)) {
            remove_nmcli_connection($s->{con_name});
        }
    }
}

# -------------------------
# Final status output
# -------------------------

sub show_manual_guest_ip_note {
    print <<'TXT';

Guest IP note:
  This script prepares networking and installs Ubuntu 24.04 cloud-image guests,
  but guest-side role configuration is still up to you.

Suggested static guest IPs on the lab network:
  routervm   : 10.10.10.10/24
  honeypotvm : 10.10.10.11/24
  node2      : 10.10.10.2/24
  node3      : 10.10.10.3/24

Suggested host bridge IP:
  labbr0   : 10.10.10.254/24

If routervm also serves a hotspot subnet through a passed-through Wi-Fi dongle,
that hotspot can use a different network such as:
  192.168.50.0/24
with routervm Wi-Fi IP:
  192.168.50.1/24

TXT
}

sub show_usb_wifi_summary {
    my @dongles = detect_usb_wifi_dongles();

    print "\nUSB Wi-Fi summary:\n";
    if (!@dongles) {
        print "  (none detected)\n";
        return;
    }

    for my $d (@dongles) {
        if ($d->{type} eq 'host_interface') {
            print "  - host interface: $d->{ifname} ($d->{desc})\n";
        }
        elsif ($d->{type} eq 'usb_device') {
            print "  - usb device: $d->{vendor}:$d->{product} $d->{desc}\n";
        }
        elsif ($d->{type} eq 'attached_to_routervm') {
            print "  - already attached to routervm: $d->{vendor}:$d->{product}\n";
        }
    }
}

# -------------------------
# Main
# -------------------------

sub main {
    require_root();
    require_tools();

    print "Taransvar lab reconciler / installer\n";
    print "====================================\n";

    handle_existing_vms();

    my %ifs = get_interfaces();
    print_interfaces(%ifs);

    my $plan = decide_topology(%ifs);
    print_plan_summary($plan);
    show_usb_wifi_summary();

    maybe_prepare_bridge_for_lab($plan, %ifs);
    maybe_create_vms();
    maybe_attach_networks_to_existing_vms();

    %ifs = get_interfaces();
    maybe_add_new_cable_nic_to_lab(%ifs);
    maybe_attach_dongle_to_routervm();
    reconcile_missing_routervm_dongles();
    reconcile_missing_bridge_slaves();

    show_manual_guest_ip_note();

    print "Current VM list:\n";
    print "  $_\n" for virsh_vm_list();

    print "\nBridge status:\n";
    my $br = run_cmd("ip addr show '$LAB_BRIDGE' 2>/dev/null");
    print($br ? "$br\n" : "(bridge not present)\n");

    print "\nLibvirt networks:\n";
    print run_cmd("virsh net-list --all") . "\n";

    print "\nDone.\n";
}

main();