#!/usr/bin/perl
#
# ./yolo_test cfg/yolov3.cfg yolov3.weights /home/ubuntu/datasets/CD2014/dataset/dynamicBackground/overpass/input/
#

$yolo="./yolo_test";
$cfg_file="./cfg/yolov3.cfg";
$weights_file="./yolov3.weights";
$dataset="/home/ubuntu/datasets/CD2014/dataset/";

opendir(DIR, $dataset);
@files = readdir(DIR);
closedir DIR;
foreach $key (@files) {
	next if($key eq "." || $key eq "..");
	next if(! -d "$dataset/$key");
	opendir(DIR2, "$dataset/$key");
	@files2 = readdir(DIR2);
	closedir DIR2;
	foreach $key2 (@files2) {
		next if($key2 eq "." || $key2 eq "..");
		next if(! -d "$dataset/$key/$key2");
		opendir(DIR3, "$dataset/$key/$key2");
		@files3 = readdir(DIR3);
		closedir DIR3;

		$dirpath = "$dataset/$key/$key2/input/";
		next if(! -d $dirpath);
		if(! -d $dirpath."/yolo") {
			mkdir($dirpath."/yolo");
		}
		#print "$dirpath\n";
		$cmd=$yolo." ".$cfg_file." ".$weights_file." ".$dirpath;
		print $cmd."\n";
		system($cmd);
	}
}
