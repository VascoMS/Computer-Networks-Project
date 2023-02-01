# Computer-Networks-Project
2022/23 Computer Networks course projects at IST.

# Project 1 - ARQ-based File Transfer System
In this project, you will learn the basics of reliable data transfer using a simplified automatic repeat request (ARQ) approach. You will be developing a simple file transfer system built on unreliable UDP messages, consisting of a client (receiver) and a server (sender).

The given required splits a file into 1000 byte chunks and sends them over the network without considering packet loss or reordering. The client recognizes the final chunk when it receives a chunk with less than 1000 bytes. Your task is to upgrade the code by implementing a sliding window ARQ algorithm to allow the receiver to rebuild the file from the segmented data even when packets are lost or reordered.

Both the sender and receiver will keep track of the file transmission using a send and receive window, respectively. The size of the windows can be specified as command line arguments. The send window on the sender keeps track of any outstanding chunks that haven't been acknowledged yet, and the receive window on the receiver keeps track of the non-contiguous sequence of chunks that have been received.

The ARQ algorithm implemented in this system is based on selective acknowledgements and timeout events. The sender only retransmits data after a timeout and uses selective acknowledgements to optimize the process and avoid retransmitting any data that has actually made it through. The system can implement any of the three common ARQ algorithms (stop-and-wait, go-back-N, and selective-repeat) by carefully choosing the window sizes.

# Project 2 - 
