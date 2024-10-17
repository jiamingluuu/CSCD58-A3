#!/bin/bash

cd pox_module
sudo python3 setup.py develop

pkill -9 sr_solution
pkill -9 sr

