killall progDelay
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 1000
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 1000
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 1000
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 1000
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 1000
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 1000
./bin/jobCommander localhost 8881 setConcurrency 4
./bin/jobCommander localhost 8881 poll
./bin/jobCommander localhost 8881 stop job_4
./bin/jobCommander localhost 8881 stop job_5
./bin/jobCommander localhost 8881 poll
./bin/jobCommander localhost 8881 exit
