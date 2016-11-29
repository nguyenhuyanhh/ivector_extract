#!/bin/bash

. path.sh

data="./workflow/data"
conf="./workflow/conf"
temp="./workflow/temp"
result="./workflow/result"
log="./workflow/log"

run.pl $log/mfcc.log compute-mfcc-feats  --verbose=2 --config=$conf/mfcc.conf scp:$data/wav.scp ark:- \| \
    copy-feats --compress=false ark:- ark,scp,t:$temp/mfcc.ark,$temp/mfcc.scp || exit 1;

echo "Computed mfcc"

run.pl $log/vad.log compute-vad --config=$conf/vad.conf scp:$temp/mfcc.scp ark,scp,t:$temp/vad.ark,$temp/vad.scp || exit 1;

echo "Computed vad"

feats="ark,ns,cs:add-deltas scp:$temp/mfcc.scp ark:- | apply-cmvn-sliding --norm-vars=false --center=true --cmn-window=300 ark:- ark:- | select-voiced-frames ark:- scp,ns,cs:$temp/vad.scp ark:- |"

ivector-extract-client "$feats"

run.pl $log/speaker_mean.log \
    ivector-normalize-length scp:$result/ivector.scp  ark:- \| \
    ivector-mean ark:$data/spk2utt ark:- ark:- ark,t:$result/num_utts.ark \| \
    ivector-normalize-length ark:- ark,scp:$result/spk_ivector.ark,$result/spk_ivector.scp 
