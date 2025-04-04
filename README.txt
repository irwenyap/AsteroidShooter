######################################## ASSIGNMENT 4 #############################################

Developed by: Irwen, Weng Khong, Yong Chin, Wilmer

Unzip folder to access executable files.

######################################## HOW TO START #############################################
1. On the Server side, click 'Host' and wait for players (clients) to connect to the game.
2. On each Client, input the Server’s IP address and port number, then click 'Connect'.
3. Once all clients are connected, click 'Start Game' on the server to begin the game.
######################################## HOW TO PLAY ##############################################

Controls:
- W, A, S, D: Move your ship
- Space: Fire bullet

######################################## INPUT PARAMETERS ##########################################

****************** Server Input Parameters ******************

a) Server port number

****************** Client Input Parameters ******************

a) Server IP address

b) Server port number


###################################################################################################
###################################### HOW IT WORKS ###############################################
- The **server controls all authoritative logic**, including:
  - Spawning asteroids

  - Collision detection

  - Score tracking

- **Clients send input only** (e.g., movement, firing bullets)

- **Collision Handling:**

  - When a bullet hits an asteroid, both are deleted across all machines.

  - The bullet’s owner (tracked via `playerID`) receives **+1 score**.

- **Scores** are tracked per player by their `NetworkID`, and displayed on the host and client UI.
###################################################################################################