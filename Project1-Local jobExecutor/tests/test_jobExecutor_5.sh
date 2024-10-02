./jobCommander issueJob ./tests/progDelay 1000
./jobCommander issueJob ./tests/progDelay 110
./jobCommander issueJob ./tests/progDelay 115
./jobCommander issueJob ./tests/progDelay 120
./jobCommander issueJob ./tests/progDelay 125
./jobCommander poll running
./jobCommander poll queued
./jobCommander setConcurrency 2
./jobCommander poll running
./jobCommander poll queued
./jobCommander exit
