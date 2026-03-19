![SOTER2](assets/SOTER.png)

### Project, that aims to create decentralized, P2P messanger for private and secure communication, with built-in anti-censorship mechanisms

## Project TODOs

- [x] Create RUDP, Peer System, gossip mechanism, etc.
- [x] Create module to link all submodules (devcore)
- [x] Enable E2EE
- [x] Create basic CLI
- [ ] Create TURN, Relay modules
- [ ] Create basic Web interface
- [ ] Create basic Android client
- [ ] Include anti-censorship mechanisms
- [ ] Add additional layer of obfuscation

## How to build

1. You need to install [libsodium](https://github.com/jedisct1/libsodium) in order to use cryptographic modules
2. You need **ABS** build system in order to build this project:

```sh
git clone https://github.com/striter-no/abs
cd abs
make && make install // automaticly moves abs to ~/.local/bin/abs
```

3. Than in root directory run `abs`, it will automaticly build all modules in project
4. After successfull build you can test modules `code/cli` and `code/stating`

### How to use

1. You need to setup _stating server_ to link your clients (`code/stating`):

```sh
cd code/stating
abs # to rebuilt, if neccarry

# listen on all interfaces, on port 9000
./bin/server 0.0.0.0 9000
```

2. To test clients you need to run module `code/cli`:

```sh
cd code/cli
abs

# automaticly connects to 127.0.0.1:9000 stating server
./bin/soter
```

> To get more exmaples, how to use soter2 interface see code/cli/examples