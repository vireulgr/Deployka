# Deployka
Set of binaries to deploy build artifacts to remote target machine via TCP connection.

## CollectArtifacts
Utility to collect all needed build artifacts according to XML config file.
Collect means to pass them to HostAgent binary to transfer via network. Or to gather them all to specific directory.
Work mode should be selected with command line options.
Intended to work on build machine (host machine).

## DeployBinaries
TBD

## HostAgent
Transfers collected binaries to target machine via TCP

## TargetAgent
Must be started on target machine. Listens for host agents to connect and transfer files.
