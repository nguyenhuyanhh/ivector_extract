# ivector_extract

## Overview

Ivector extraction in Kaldi is done through invoking 3 programs running in a pipe: gmm-gselect, fgmm-global-gselect-to-post and ivector-extract. Each program takes the input (features generated from wave files), certain models and the output from previous program to generate output. Models are large files (~400MB), the overhead is negligible when processing thousands of features at once, however in the use case of processing one feature at once the overhead is significant.

The code merges 3 programs into one, load the models at the beginning and then act as a server, processing a feature from a request and return the results.

## Setup

1. Clone the project
2. Run the Kaldi patch script: `$ ./patch_kaldi.sh /path/to/kaldi/root`

## Structure of the repository

```
src/
	ivector-extract-server.cc 	# server code
	ivector-extract-client.cc 	# client code
bin/
	ivector-extract-client		# client binary
workflow/
	conf/						# config for compute-mfcc and compute-vad
		mfcc.conf
		vad.conf
	models/
		final.ie
		final.ubm
		[final.dubm] 			# optional
	data/
		utt2spk					# mapping of utterance id to speaker id
		spk2utt 				# the reverse of above (usually has the same content)
		file_name.sph 			# the source sph file
		wav.scp 				# Kaldi script file to convert sph to wav for processing 
	temp/ 						# folder to store temp result files
		mfcc.ark
		mfcc.scp
		vad.ark
		vad.scp
	result/ 					# folder to store results
		ivector.ark
		ivector.scp
		num_utts.ark
		spk_ivector.ark
		spk_ivector.scp
	gold/ 						# gold standard if exists
	log/						# folder to store log files
		mfcc.log
		vad.log
		speaker_mean.log
	utils/ 						# utilities, only run.pl needed
		run.pl
patch_kaldi.sh 					# patch Kaldi with ivector-extract-server
path.sh 						# include paths
server.sh 						# start server
client.sh 						# start client
terminate.sh 					# client shortcut to terminate server
```

## To-do

1. Only need to take sph file, generate the rest of the input files automatically	
