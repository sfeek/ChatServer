Chat Server
=

There is currently a chat client available but any telnet client will do. Just connect to the server on the specified port and address. By default port 6969 is used.

## Build
Run GNU make in the repository
`make`

Then start
`./chat_server`

## Features
* Accept multiple clients (up to 100 by default)
* Name and rename users
* Send private messages
* Multiple Rooms
* Perform basic math functions
* Roll Dice
* Emote
* Mute
* Ring Terminal Bell

## Chat commands

| Command       | Parameter             |                                     |
| ------------- | --------------------- | ----------------------------------- |
| \quit         |                       | Leave the chatroom                  |
| \ping         |                       | Test connection, responds with PONG |
| \nick         | [nickname]            | Change nickname                     |
| \pm           | [user] [message]      | Send private message                |
| \who          |                       | Show active clients                 |
| \help         |                       | Show this help                      |
| \math         | [expression]          | Math in the terminal                |
| \room         | [room_name]           | Join another room                   |
| \time         |                       | Show current server time            |
| \echo         | [on/off]              | Turn local echo on/off              |
| \me           | [message]             | Emote                               |
| \roll         | [die_sides]           | Roll Dice                           |
| \away         | [short_message]       | Set Away status                     |
| \bell         | [nickname]            | Ring Terminal Bell                  |
| \mute         | [nickname_list]       | Mute listed nicknames               |
