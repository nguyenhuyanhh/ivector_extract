#!/bin/bash

. path.sh

num_gselect=20 # Gaussian-selection using diagonal model: number of Gaussians to select
min_post=0.025 # Minimum posterior to use (posteriors below this are pruned out)

models="./workflow/models"
data="./workflow/data"
temp="./workflow/temp"

# gmm="fgmm-global-to-gmm $models/final.ubm -|"
gmm="$models/final.dubm"
fgmm="$models/final.ubm"
ivector="$models/final.ie"

tmp="ark,scp,t:$temp/ivector.temp.ark,$temp/ivector.temp.scp"

ivector-extract-server --verbose=2 --n=$num_gselect --min-post=$min_post "$gmm" "$fgmm" "$ivector" "$tmp" || exit 1;
