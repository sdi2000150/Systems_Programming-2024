./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 1000
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 110
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 115
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 120
./bin/jobCommander localhost 8881 issueJob ./tests/progDelay 125
./bin/jobCommander localhost 8881 poll
./bin/jobCommander localhost 8881 setConcurrency 2
./bin/jobCommander localhost 8881 poll
./bin/jobCommander localhost 8881 exit
