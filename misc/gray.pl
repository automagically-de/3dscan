#!/usr/bin/env perl

use strict;

use Image::Magick;

my $bits = 6;
my $perimeter = 37.7;
my $height = 2;

sub gray_code {
	my $i = shift;
	return $i ^ ($i >> 1);
}

my $f = 300 / 2.54;
my $w = $perimeter * $f;
my $h = $height * $f;

my $image = Image::Magick->new('size' => "$w"."x"."$h",
	'background' => 'white');
$image->ReadImage('xc:white');
$image->Border('color' => 'black', 'height' => 1);

my $n = 2 ** $bits;
my $w1 = $w / $n;
my $h1 = $h / $bits;

for(my $i = 0; $i < $n; $i ++) {
	my $gray = gray_code($i);
	for(my $b = 0; $b < $bits; $b ++) {
		if($gray & (1 << $b)) {
			my $x1 = $w1 * $i;
			my $x2 = $x1 + $w1;
			my $y1 = $h1 * $b;
			my $y2 = $y1 + $h1;
			$image->Draw(
				'stroke' => 'black',
				'primitive' => 'rectangle',
				'points' => "$x1,$y1,$x2,$y2");
		}
	}
	#printf "%03d: %0*b\n", $i, $bits, $gray;
}

my $x = $image->Write('gray.png');
warn "$x" if $x;

