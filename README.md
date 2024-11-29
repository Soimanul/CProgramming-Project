## Please, make sure to run the following commands by running cd ChatApp to ensure you are in the correct directory 


# Compiling for Windows
    Compile server
        - gcc -o server server.c -lws2_32 
    Compile client
        - gcc -o client client.c -lws2_32  

# Compiling for other Operating Systems
    Compile server
        - gcc -o server server.c 
    Compile client
        - gcc -o client client.c

## Running program 
    Running server (run only once)
        - Make sure to replace your current Wireless LAN IPv4 Address in client.c
        - ./server
    
    Running client (you can run several clients from different terminals)
        - ./client LAN

