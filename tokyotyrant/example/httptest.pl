#! /usr/bin/perl

use strict;
use warnings;

use LWP::UserAgent;

my $ua = LWP::UserAgent->new(keep_alive => 1);
my $baseurl = 'http://localhost:1978/';


foreach my $i (1..100){
    my $req = HTTP::Request->new(PUT => $baseurl . $i, [], "mikio:$i");
    my $res = $ua->request($req);
}
for(my $i = 1; $i < 107; $i += 7){
    my $req = HTTP::Request->new(DELETE => $baseurl . $i);
    my $res = $ua->request($req);
}
for(my $i = 1; $i < 100; $i += 3){
    my $req = HTTP::Request->new(POST => $baseurl . $i, [], "hirabayashi:$i");
    my $res = $ua->request($req);
}
foreach my $i (1..107){
    my $req = HTTP::Request->new(GET => $baseurl . $i);
    my $res = $ua->request($req);
    printf("%d: %s\n", $i, $res->content) if($res->is_success());
    $req = HTTP::Request->new(HEAD => $baseurl . $i);
    $res = $ua->request($req);
    printf("%d: %s\n", $i, $res->header("content-length")) if($res->is_success());
}
