#!/bin/bash

#Run polls and redirect their output into txt files
./jobCommander poll running > running_jobs.txt
./jobCommander poll queued > queued_jobs.txt

#Read the running_jobs.txt file line by line
while read LINE                 #read up to '\n'
do
    rest_string=${LINE:1}       #remove first "<" from the "<job_XX,job,queuePosition>"
    job_ID=${rest_string%%,*}   #remove everything from the first "," and after

    #Stop each job_ID
    if [ -n "$job_ID" ]; then   #to skip possible empty entries 
        ./jobCommander stop "$job_ID";
    fi
done < running_jobs.txt

#Read the queued_jobs.txt file line by line
while read LINE                 #read up to '\n'
do
    rest_string=${LINE:1}       #remove first "<" from the "<job_XX,job,queuePosition>""
    job_ID=${rest_string%%,*}   #remove everything from the first "," and after

    #Stop each job_ID
    if [ -n "$job_ID" ]; then   #to skip possible empty entries 
        ./jobCommander stop "$job_ID";
    fi
done < queued_jobs.txt

#Delete temporary txt files
rm running_jobs.txt queued_jobs.txt