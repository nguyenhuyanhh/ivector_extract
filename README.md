# ivector-extract-server

## Overview

Ivector extraction in Kaldi is done through invoking 3 programs running in a pipe: gmm-gselect, fgmm-global-gselect-to-post and ivector-extract. Each program takes the input (features generated from wave files), certain models and the output from previous program to generate output. Models are large files (~400MB), the overhead is negligible when processing thousands of features at once, however in the use case of processing one feature at once the overhead is significant.

The code merges 3 programs into one, load the models at the beginning and then act as a server, processing a feature from a request and return the results.

## Structure of the repository

```
kaldi/ # Kaldi installation
	src/ 
		ivectorbin/
			ivector-extract-server.cc # server code
			Makefile # updated Makefile including the server code
			...
		...
	...
ivector-extract-client/
	ivector-extract-client.cc # client code
	Makefile # for the client
workflow/
	models/
		final.ie
		final.ubm
		[final.dubm] # optional
	data/
		feats.scp # one line, containing the feature name and the path to the raw ark file
		vad.scp # same as above
		raw_mfcc_....ark # ark file pointed to by feats.scp
		vad....ark # ark file pointed to by vad.scp
		spk2utt # mapping of speaker and utterance
	temp/ # folder to store temp result files
	result/ # folder to store results
	gold/ # gold standard if exists
	log/
		speaker_mean.log # log file
	utils/
		run.pl
		...
path.sh # include paths
server.sh # start server
client.sh # start client
terminate.sh # terminate server
```


		
