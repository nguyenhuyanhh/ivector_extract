#!/bin/bash

. path.sh

num_gselect=20 # Gaussian-selection using diagonal model: number of Gaussians to select
min_post=0.025 # Minimum posterior to use (posteriors below this are pruned out)

models="./workflow/models"
data="./workflow/data"
result="./workflow/result"

# gmm="fgmm-global-to-gmm $models/final.ubm -|" # if there is no final.dubm
gmm="$models/final.dubm"
fgmm="$models/final.ubm"
ivector="$models/final.ie"

results="ark,scp,t:$result/ivector.ark,$result/ivector.scp"

ivector-extract-server --verbose=2 --n=$num_gselect --min-post=$min_post "$gmm" "$fgmm" "$ivector" "$results" || exit 1;
