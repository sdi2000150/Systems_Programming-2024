#!/bin/bash
arguments="$*"  #save default variable $*, which is the arguments at this point

for commands_file in $arguments #for every argument-file
do
    echo "Processing file: $commands_file"
    #Read the input file line by line
    #read up to '\n'. IFS to prevent from trimming special chars. -r to treat '\' as literal char
    while IFS= read -r LINE
    do
        echo "Executing command: $LINE"
        ./bin/jobCommander localhost 8881 issueJob "$LINE"
    done < "$commands_file"
done
