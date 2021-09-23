#!/bin/bash

i=0;
while ((i < 100)); do
  ./client localhost 8080 POST "ls -l" &
  i=$((i+1))
done

