# Campfire plugin for Pidgin

**WARNING: This software is untested and may not work.**

If you’re still reading, then you take responsibility for what the
software may do to your computer. (In other words, I probably can’t help
you fix anything that blows up.)

If you’re still reading, then follow these instructions (for Linux Mint;
other flavors of linux or distros…YMMV):

If you don’t already have them, install the standard compile and make
packages for linux. (I forget what they are. Look them up.)

## Install Purple

Install the development headers for libpurple (Ubuntu):

```bash
sudo apt-get install libpurple-dev
```

## Download & Install

```bash
mkdir -p campfire-libpurple
cd campfire-libpurple
curl -L https://github.com/jrfoell/campfire-libpurple/archive/master.tar.gz | tar -xz --strip=1
make
sudo make install
```

## Pidgin configuration

**Restart Pidgin.**

Now that it installed, here’s how to configure it for your account:

- Open “Manage Accounts”.
- Click “New…”
- Select the “Campfire” protocol.
- Enter your username and the hostname of your campfire server.
- Get an API key from the “my info” link on your campfire server’s web interface. (In short: use a web browser to get this info.)
- Enter the API key in the “Advanced” tab.
- Save the account.

Theoretically, you should now be able to chat on Campfire using Pidgin. Here’s how to join a room:

- In the Pidgin Buddy List, select menu item Buddies > “Join a chat…”
- Select the “Campfire” protocol from the dropdown list.
- Click “Room List” to get a list of rooms available on your Campfire server.
- Select the room you wish to join, then click “Join Room”. A new chat window should open up to that room.
- You may have to close the “Join a chat” dialog box.

Initial instructions from: http://opensourcemissions.wordpress.com/2013/02/13/how-to-install-the-campfire-plugin-for-pidgin-on-linux/
