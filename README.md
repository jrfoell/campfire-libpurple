A campfire plugin for Pidgin, Adium and other libpurple based messengers.

# Building
    $ sudo apt-get install build-essential make libpurple-dev git-core
    $ git clone git://github.com/jrfoell/campfire-libpurple.git
    $ cd campfire-libpurple
    $ sudo make
    $ sudo make install

And now, restart Pidgin.

# Configuration
Now that it installed, here’s how to configure it for your account:

 * Open “Manage Accounts”.
 * Click “New…”
 * Select the “Campfire” protocol.
 * Enter your username and the hostname of your campfire server.
 * Get an API key from the “my info” link on your campfire server’s web interface. (In short: use a web browser to get this info.)
 * Enter the API key in the “Advanced” tab.
 * Save the account.


Theoretically, you should now be able to chat on Campfire using Pidgin. Here’s how to join a room:

 * In the Pidgin Buddy List, select menu item Buddies > “Join a chat…”
 * Select the “Campfire” protocol from the dropdown list.
 * Click “Room List” to get a list of rooms available on your Campfire server.
 * Select the room you wish to join, then click “Join Room”. A new chat window should open up to that room.
 * You may have to close the “Join a chat” dialog box.

# Contributors
* guitarmanvt - this help
* jrfoell
* mtseinart

