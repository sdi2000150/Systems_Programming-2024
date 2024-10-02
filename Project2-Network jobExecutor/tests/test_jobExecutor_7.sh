killall progDelay
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 1000
./bin/jobCommander localhost 8881 stop job_2
./bin/jobCommander localhost 8881 exit
