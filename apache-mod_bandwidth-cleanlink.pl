#!/usr/bin/perl

#
# A little daemon that I use to clean links created by the bandwidth
# module for Apache when they weren't removed properly by the server.
# (Ie: when a httpd process didn't terminated in the usual way.)
#
# Change the value of TIME to how often (in seconds) you want to
# do the cleaning.
#

$TIME=120;

$PS="ps auxw";
$LINKDIR="/var/run/apache-mod_bandwidth/link";


sub do_clean {
   local(%ppid);

   open(INP, "$PS |");
   while(<INP>) {
      if (($process) =/^\S+\s+(\d+)\s+.+httpd\s/) {
         $ppid{$process}=1;
      }
   }
   close(INP);

   opendir(DIR, $LINKDIR);
   local(@filename)=readdir(DIR);
   closedir(DIR);

   for($i=0; $i <= $#filename; $i++)  {
      if ($filename[$i] =~ /^\d+$/) {
         if (! $ppid{$filename[$i]}) {
            unlink("$LINKDIR/$filename[$i]");
         }
      }
   }
}

do_clean();

