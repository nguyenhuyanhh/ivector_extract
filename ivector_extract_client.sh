#!/bin/bash

. path.sh

cmd=run.pl

data="./workflow/data"
temp="./workflow/temp"
result="./workflow/result"
log="./workflow/log"

feats="ark,ns,cs:add-deltas scp:$data/feats.scp ark:- | apply-cmvn-sliding --norm-vars=false --center=true --cmn-window=300 ark:- ark:- | select-voiced-frames ark:- scp,ns,cs:$data/vad.scp ark:- |"

ivector-extract-client "$feats"

cat $temp/ivector.temp.scp > $result/ivector.scp
cp $temp/ivector.temp.ark $result/ivector.ark

$cmd $log/speaker_mean.log \
    ivector-normalize-length scp:$result/ivector.scp  ark:- \| \
    ivector-mean ark:$data/spk2utt ark:- ark:- ark,t:$result/num_utts.ark \| \
    ivector-normalize-length ark:- ark,scp:$result/spk_ivector.ark,$result/spk_ivector.scp 
