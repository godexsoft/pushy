#!/bin/bash
git pull && git submodule init && git submodule update
cd lib/push_service
git checkout master
git pull
cd -
