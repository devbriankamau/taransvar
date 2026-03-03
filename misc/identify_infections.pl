#!/usr/bin/perl
use strict;
use warnings;
use DBI;
use lib ('.');
use func;	#NOTE! See comment above regarding lib..
use lib_cron;


# Connect to MySQL database
my $dbh = getConnection();

#Get other connection for lookup
my $lookupDbh = getConnection();

# Prepare and execute query to scan the table
my $query = "SELECT reportId, inet_ntoa(ip) as ip, ip as nIp, port, status, sentByIp as nSentBy, inet_ntoa(sentByIp) as aSentBy, adminIp, inet_ntoa(globalDb1ip) as globalDb1ip, inet_ntoa(globalDb2ip) as globalDb2ip, inet_ntoa(globalDb3ip) as globalDb3ip FROM hackReport, setup where handledTime is null";
my $sth = $dbh->prepare($query);
$sth->execute();
my $nFound = 0;

# Fetch and print each row
while (my $row = $sth->fetchrow_hashref()) {
#    print "Row:\n";
        $nFound++;
        print "   $row->{'ip'} $row->{'port'} $row->{'status'}\n";

        if ($row->{'nSentBy'} == $row->{'adminIp'}) {
                #config_update.php is used by honey.php to report hack attempts on the same computer (sentByIp will be this)
                print "Internal hack report (maybe from script/config_update.php)\n";

                #Report to the IP address owner (partner). 
                my $szLookup = "select partnerId from partnerRouter where ip = ?";
                my $sth = $dbh->prepare($szLookup);
                $sth->execute($row->{'nIp'});

                if (my $partnerRow = $sth->fetchrow_hashref()) {
                        #Partner found.. Send to partner and global DB server
                        my $szUrl = "http://".$row->{'ip'}."/script/config_update.php?f=report&ip=".$row->{'ip'}."&port=".$row->{'port'}."wt=honeypot";
                        print "Reporting to owner:\n".$szUrl."\n";
                        my $szReply = getWgetResult($szUrl);
                        print "Reply from owner:\n".$szReply."\n";

                        #Report to global DB server
                        for (my $n = 1; $n<=3; $n++)
                        {
                                my $szDbServer = 'globalDb'.$n."ip";
                                if ($row->{$szDbServer}) {
                                        my $szUrl = "http://".$row->{$szDbServer}."/script/config_update.php?f=report&ip=".$row->{'ip'}."&port=".$row->{'port'}."wt=honeypot";
                                        print $szUrl;
                                        my $szReply = getWgetResult($szUrl);
                                        print $szReply."\n";
                                }
                        }
                }
                else {
                        print "Partner not found: ".$row->{'ip'}." (".$row->{'nIp'}.")\n";
                }

                #Set to handled (not yet)
                my $szSql = "update hackReport set handledTime = now() where reportId = ?";
                my $sth = $dbh->prepare($szSql);
                $sth->execute($row->{'reportId'});
        }
        else
        {
                #probably a report from global DB server that one of my units has attempted hacking
                my $szLookup = "select portAssignmentId, ipAddress, inet_ntoa(ipAddress) as aIp from unitPort where port = ".$row->{'port'}." order by created desc limit 1";
                print "\n$szLookup\n";
        
                my $sth = $lookupDbh->prepare($szLookup);
                $sth->execute();
                while (my $lookupRow = $sth->fetchrow_hashref()) {
                        #    print "Row:\n";
                        print "Internal IP found: ".$lookupRow->{'aIp'}."\t".$lookupRow->{'portAssignmentId'}."\t".(defined($lookupRow->{'status'})?$lookupRow->{'status'}:"No status")."\n";
                        #$szLookup = "select unitId, coalesce(description, hostname, hex(dhcpClientId)) as desc from unit where ipAddress = ? order by    
                }
        }
}

if (!$nFound) {
        print "No record found!\n";
}



# Insert new records into the table
#my $insert_sql = "INSERT INTO internalInfections (ip, nettmask, status) VALUES (?, ?, ?)";
#my $sth = $dbh->prepare($insert_sql);

# Example data to insert
#my @data = (
#    ['Alice', 25],
#    ['Bob', 30],
#    ['Charlie', 28],
#);




# Clean up
#$sth->finish();
#$dbh->disconnect();





#foreach my $row (@data) {
#    $sth->execute(@$row) or die $DBI::errstr;
#    print "Inserted: @$row\n";
#}

# Disconnect from the database
#$dbh->disconnect;
#print "Done.\n";

