#!/bin/bash

#Run polls and redirect their output into txt files
./bin/jobCommander localhost 8881 poll > queued_jobs.txt

#Read the queued_jobs.txt file line by line
while read LINE                 #read up to '\n'
do
    rest_string=${LINE:1}       #remove first "<" from the "<job_XX,job>"
    job_ID=${rest_string%%,*}   #remove everything from the first "," and after

    #Stop each job_ID
    if [ -n "$job_ID" ]; then   #to skip possible empty entries 
        ./bin/jobCommander localhost 8881 stop "$job_ID";
    fi
done < queued_jobs.txt

#Delete temporary txt file
rm queued_jobs.txt