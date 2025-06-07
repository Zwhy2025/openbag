#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"


bash $SCRIPT_DIR/install_base.sh
bash $SCRIPT_DIR/install_dev.sh
bash $SCRIPT_DIR/install_runtime.sh
bash $SCRIPT_DIR/install_graphics.sh
bash $SCRIPT_DIR/install_conda.sh 

rm -rf /tmp/*
rm -rf /var/lib/apt/lists/*
rm -rf /var/cache/apt/*
rm -rf /var/cache/apt/archives/*