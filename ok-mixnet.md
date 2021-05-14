I'm a human being, and sometimes I'd like to send someone a message. I don't want anyone other than me and whoever the message is for to read it--not strangers, not companies, and not governments. It's possible, and with computers, it should be easy and instant.

# OK-Mixnet
OK-Mixnet is my proposed sketch for a method to send small messages, slowly. It's designed for things like email, IRC-style slow chatrooms, or bulletin boards. In my experience, this is the same stuff I'd use GPG for.

OK-Mixnet sends
- **small messages**: To keep it simple for the proof-of-concept, messages are simple plain text letters. They're broken up into binary chunks of 1K or less. Bigger letters get split up and sent in parts.
- **slowly**: How slow depends on some settings and the network size. With only 100 people in the network, it will take 2 minutes to deliver. With 100K people, it will take more like 2 days.
- **to people you already know**: Messages are always sent from you to one friend you know already. You and each of your friends need to pre-exchange a file full of random bits in person--this is the basis of the securely talking to that friend.
- **with OK security**
    - You need a very good source of random numbers, such as a hardware random number generator.
    - No one can read or modify a message betwen you and your friends, if the computers and environments on either end are secure. This is guaranteed by math.
    - I've tried to design it so that it's pretty hard to tell who's sending a message to who, when the messages are sent, if there are messages being sent at all, or which people know each other. I offer no mathematical guarantees on these parts.
    - It's pretty easy to just shut down the whole mixnet (denial of service), and it's also possible to also shut down messages between two friends if you know who they are.
    - Obviously, it's not magic. If your computer is hacked, your friend's computer is hacked, or someone puts a listening device in your keyboard, your messages may become public. If you send your one-time pad over the internet instead of exchanging it in person like you should, your messages may become public. If you generate your one-time pad using /dev/urandom, your messages may become public. If you get a message about something and immediately copy-paste it into an email, the fact that you learned it from someone on Mixnet may become public. If someone shuts down communication betwen you and your friend, and you call them up, they will know you were trying to communicate on OK-Mixnet.
- **in a way mortals can understand**
    - OK-Mixnet is designed to be secure in a way that my 14-year old self can understand that it's secure, and why. There is (almost) no algebra, trust in experts, or unproven hardness assumptions. All you need is a very good random number generator.
    - This doesn't mean a random person off the street can understand it--you'll probably still have to be a bit smart to follow mathematical proofs, or to understand possible attacks. But I think all of that is still accessible to smart teenagers.
    - The main exception is the one-time MAC explained below, which uses modulo arithmetic and 8th-grade algebra. Understanding the math proofs of why it works however is a bit harder, and involves a proof with primes.

Here are the drawbacks:
- It's slow. Really slow.
- You have to exchange keys, with every person you want to talk to. In person.
- You have to find a source of random numbers you trust. This might mean making something or buying a hardware device. But if you trust Linux's /dev/urandom, you can use that too.
- No one else is using it. Also, it's designed to hide who's communicating, but there's probably only going to be 10 people on it, so that means people will know you got a message from one of those 10 people.

Background:
- [Perfect Security](#perfect_security) (or, why should we exchange keys in person)
- todo -- SIGINT (or, why do we need to hide when messages are sent and between whom?)
- probably not todo -- Finite Field Arithmetic
- probably not todo -- why p=2^9689-1, and proof that it's a prime

I'll explain how it works iteratively. 
- The Perfect Message protocol securely sends a 1K message from person A to person B, but doesn't hide the messages at all.
- The OK-Link protocol securely sends a 1K message from person A to person B, and tries to hide when the message is being sent.
- The OK-Mixnet protocol securely sends 1K messages, and tries to hide when messages are being send and between whom.
- todo -- The proof-of-concept client and interface is left as an exercise to the programmer (me!).

## One-Time Pads (Perfect Encryption)
- You're reading a document about mixnets so you know what a [one-time pad](https://en.wikipedia.org/wiki/One-time_pad) is already.
- People used to use mod-26 addition with letters. Now we use xor with binary bits. Any mod-N addition works.
- It has perfect security against passive attackers. You should prove to yourself that it's actually perfect if you haven't. Don't take things on faith.
- It's also not secure against active attackers. An attacker can easily swap letters if they know or guess parts of the original message.
- You should never re-use a one-time pad, or all the security guarantees are gone.

## One-Time MACs (Perfect Authentication)
- See [http://web.mit.edu/6.857/OldStuff/Fall97/lectures/lecture3.pdf](Ron Rivest's) lecture notes. I'm not sure if he's the original author or not.
- Again, you can't re-use one-time keys or the security breaks. Again, it's perfect security. Again, the key is annoyingly long. This is probably the best you can do?
- This combines with One-Time Pads to make a one-time pad secure against active attackers.
- There is a magic number p, we have to do all arithmetic mod p. For One-Time Pads, p didn't matter. For One-Time MACs, we need p to be a prime. This is basically so that multiplication and division work the way you're used to. For more information, research finite fields.

# The Perfect Message Protocol
The Perfect Message Protocol sends a message between person A and person B, who have exchanged some secret numbers ahead of time. The message stays private and can't be tampered with, but anyone can see the message being sent.

To send the message
1. Our inputs are 
    - A 9688-bit (1211-byte) message.
    - A 29066-bits random pad, which we shared ahead of time. We split this into:
        - A 9688-bit number, OTPK (one-time pad key)
        - Two 9689-bit random numbers, OTMK-a and OTMK-b (one-time mac keys). Neither is allowed to be 9689 consecutive 1 bits--if it is, throw it out and make a new one, and also throw out your random number generator and make a new one.
    - We always set p to the constant value (2^9689)-1, which is a large prime number.
2. Calculate the 9688-bit `ciphertext = (message) xor (OTPK)`
3. Calculate 9689-bit `message authentication code = [(OTM-a) * (ciphertext) + (OTM-b)] modulo p`. Notice that this is addition and multiplation of 9689-bit numbers, so this isn't automatically handled by your CPU. But it's not hard either.
4. Send ciphertext and MAC to the recipient.

To receive the message:
1. Our input is
 - A 9688-bit (1211-byte) ciphertext.
 - All the same pre-shared random bits: the OTPK, OTMK-a, and OTMK-b.
 - The same p = (2^9689)-1, a constant large prime number
2. Calculate 9689-bit `expected message authentication code = [(OTM-a) * (ciphertext) + (OTM-b)] modulo p`. 
3. If expected and received authentication codes don't match, the message has been tampered with (or there was some other bug and you have bad data). Report a failure and exit immediately.
4. Otherwise, calculate the 9688-bit (1211-byte) `plaintext = (ciphertext) xor (OTPK)`
5. Return the plaintext (message), which has been received tamper-free.

# The OK-Link Protocol
The OK-Link protocol sends short messages between person A and person B, who have exchanged some secret files filled with random bits ahead of time. The messages are private and can't be tampered with, and adversaries can't tell when messages are being sent.

There is a transmission size and period. The defaults are a 1K message (size) per 1 minute (period).

1. First, two friends generate large files of random bits. 40GB of data is suggested. They exchange the bits with each other, for example by DVD-ROM or trusted USB sticks. Internally, the OK-Link xors the two 40GB files together to form a single 40GB one-time pad. The original files can be deleted. 
2. The friends also agree on a start time a little in the past--say midnight of the current date. They write down the date in a text file, which is stored along with the two pads.
3. The file is then divided into a bunch of individual one-time pads, which are numbered--each one will be used for a different message. For the first minute after the start time, pad 1 is used, for the second minute pad 2 is used, and so on.
4. Whenever you want to send a message, it gets added to a message queue.
5. There is one modification from the Perfect Message protocol--a unix timestamp is put at the start of the message, before the encrypted bits. This is in case the sender and receiver clock are a little off, so they can agree on which one-time pad should be used.
6. Once a minute, both A and B send each other a Perfect Message. There are two main kinds of perfect messages sent, C and not-C. All messages are zero-padded at the end to reach 1211 bytes.
    - **C / Chaff**: 'C' messages are ignored and deleted on receipt.
        - Format: The byte "C" meaning chaff; 1210 zero bytes of padding.
    - **M / Message**: Arbitrary binary data up to 1000 bytes, to be shown to the user. For testing I recommend ASCII text.
        - Format: The byte "M"; 4-byte message id; 2-byte length; up to 1000 bytes of message
    - **P / Partial Message**: For long messages. All the parts are automatically concatenated together, and then shown to the user at the end.
        - Format: The byte "P"; 4-byte messsage id; 4-byte number of parts; 4-byte part number; up to 1000 bytes of message
    - **R / Received**: Read receipts/confirmation. Sent to the client to say a message was successfully received. Can be sent when convenient.
        - Format: The byte "R"; 2-byte message count; 4-byte message id for each message
    - **X / Experimental**: Please use this if you want to make a custom client and make your own messages.
        - Format: The byte "X"; 10-byte ascii name for your message type (pick anything unique, pad with zeros); your custom format of 1200 bytes

Security:
- A message of exactly 1K is sent exactly once, every minute, whether or not there are messages in the queue.
- It should be hard to tell if or when a message is being sent based on traffic patterns
- For security reasons, messages should be prepared in advance of the minute mark, to avoid more fine-grained timing attacks.
- Wildly wrong timestamps should be rejected by the reciever. 5 minutes off is fine.
- The properties of one-time pads and one-time MACs means it should not be possible to tell any contents from any others, including chaff messages. There's no strong reason to use zero padding versus random padding.
- If you xor a random stream against anything chosen without seeing the stream first, you get a random stream. If two friends xor things together, you should get good one-time pads as long as ONE of them has a good HRNG, even if the other is actively sabotaged (incidentally, this is how I made my HRNG system--I xor three untrusted generators together for extra oompf).

At the default rates, a 40GB file of random data will work for 10 years (6K is used per message pair).

# The OK-Mixnet Protocol
The OK-Mixnet protocol is almost the same as the OK-Link protocol. Let's say there are 100 people who want to communicate. This is the network size. Some of them are horrible spammers, some are your friends. What we essentially do is to open 99 OK-Link channels, one to each other person in OK-Mixnet, and transmit on all of them. BUt to make traffic less bursty, we stagger the transmissions, and transmit to each person in turn instead.

1. Exchange large files of random bits with each of your friends, as in OK-Link, and form a one-time pad. This can be done after the network is already running, you just won't be able to decode messages until then.
2. Get a list of the ID and IP address of everyone in the mixnet. This is done by a central authority for the proof-of-concept--I'll maintain a file by hand and email it to a mailing list. Your ID is a unique number between 0 and 99 (between 0 and N-1)
3. You equip your machine with a good hardware random number generator.
Every second, you transmit a Perfect Message to id `[(unix timestamp in seconds) + (your ID)] modulo (network size)` and receive a message.
    - To transmit a message to a friend, you use exactly the protocol in OK-Link
    - To receive a message from a friend, you use exactly the protocol in OK-Link. Remember, you should be prepared to receive messages a little off--clocks don't always agree.
    - You can't transmit a message to a stranger, but to make it look like you are, you send a unix timestamp followed by random bits of the same length as a message instead.
    - Discard all messages from strangers without reading them.

If there are 100 people in OK-Mixnet, you contact your friend with a message every 99 seconds, or every 2 minutes. The total traffic sent over the network is 6K/second (15GB/month). The more people join OK-Mixnet, the slower it gets to send messages, but the bandwidth usage per person stays the same.

Security
- Messages are sent on a scheduled basis, so it should be near-impossible to tell when you are talking to whom.
- The security works against ANY number of malicious nodes. There is no majority-good requirement. If the Russian government adds 1000 nodes to the network, it will make it slower, but it shouldn't affect the security.
- As in OK-Link, message processing needs to be fast enough to prevent timing attacks. I suggest doing it ahead of time.
- It's VERY important that the random number generators used for chaff and pads are good enough. If your random generator is bad enough, an adversary can tell who your friends are, or even which messages are chaff. Even if your generator and your friends' generator is good, if 80% of the mixnet's generators are bad, you're now hiding in a group 20% the size. 
- If the computerized client gets an invalid message from a friend, or doesn't get a message on schedule, it should raises a red flag to the user that an attacker might be present. Too many networked security programs silently ignore errors instead of raising them to user attention.

# Notes and Known Improvements
- In the proof-of-concept, the entire network needs to be shut down and restarted when someone joins or leaves OK-mixnet.
- It would probably be reasonable to add broadcast messages, which could be used as the basis for things like bulletin boards or larger group chats.
- For pads, it seems like you'd like to only consume the pad while sending messages, and only consume a few bytes per chaff message. But I prefer the current simple system. There are some problems with active attacks and most obvious proposals to avoid waste. It would introduce more complexity than you think to do it well.
- A note on random-number sources. I've mentioned, you need a really good source. The good news is that it's easy to make a pretty good random number generator, because of the properties of xor and hashes--you don't need all your sources to be perfect, you just to estimate the amount of randomness conservatively enough. New Intel CPUs support a [RDRAND](https://en.wikipedia.org/wiki/RDRAND) instruction that generates random numbers (at [500MB/s](https://stackoverflow.com/questions/10484164/what-is-the-latency-and-throughput-of-the-rdrand-instruction-on-ivy-bridge)), but it's hard to check if the output is good, and harder to check if it's backdoored. Just take a few sources like this where the output is decent (CPU, an external USB device, a software PRNG) and xor them together, and you get a good source of random bits.
- That all said, I could probably do more to mitigate bad random number generators. Right now distinguishing chaff messages is too easy. I'm open to suggestions.
- As a note, it's possible to substitute public crypto and PRNG-based pads, to avoid in-person exchanges, and end up with a similar system. But I feel this defeats the whole point of OK-Mixnet, so I won't support it in my own code.

# Background
## <a name="perfect_security"> Background: Perfect, or Information-Theoretic Security
I'll explain what perfect security is, as a motivator for why you would want to do this, even though you need to exchange keys in person. Feel free to skip ahead to "One-Time Pads" if this isn't your thing.

There are three levels of security you can get in the theoretical digital world.
- Perfect (information-theoretic): If you send a bunch of bits, people can stare at the bits all they want, and they will never have any idea what the bits are. Not even one of them. Not even a little. Not even with a supercomputer.
- Computationally difficult: It costs an attacker more effort to break a code than it takes you to send a message in the code. Usually there's a pretty big multiplier, so that it's relatively easy to make something that all of human civilization couldn't break in the next 20 years, by scaling up the size a little.
- Practically difficult: It's hard, and people have tried to break it, but we don't actually have any mathematical proof it will be hard. We just have a lot of evidence that no one has broken it, because several smart people tried for decades, and no one has come forward and said "I did it! I broke it!".
- Quantum entanglement?: I don't understand quantum computing well enough to talk about it much, but there might be actually good quantum security? Ask me again in five years, It doesn't seem practically useful until everyone does, but it's worth thinking about beforehand.

Today, we don't know anything in the "compuationally difficult" class--humankind still hasn't figured out, fundamentally, whether any computationally difficult crypto problems exist (the existence of [one-way-functions](https://en.wikipedia.org/wiki/One-way_function) ). We _suspect_ that some of the problems we know are practically difficult, might also be computationally difficult--we just can't prove it.

On the other hand, we do know a couple perfect security methods, which I'll cover. Given the choice to use provably perfect security any smart person can verify themselves, I would rather use perfect security. So why don't people?

Why people don't use perfect security
- Convenience. In general, public-key (asymmetric) cryptosystems are easier to build infrastructure on than private-key (symmetric) cryptosystems. I'm not sure, but I think asymmetric perfect crypto is impossible.
- Perfect security [requires](http://pages.cs.wisc.edu/~rist/642-fall-2012/shannon-secrecy.pdf) large keys (as large as the messages) which can't be re-used. The re-use thing is a little bad, but computer storage is cheap, and this doesn't seem like a big deal to me.
- Some of the approaches given here are not well-known, such as signing
- It requires a good source of random numbers

Problems that CAN be solved using perfect crypto:
- Private key (pre-shared key) encryption, against passive eavesdropping. Covered under One-Time Pads.
- Signing, or proving that a message is from who it says, to the intended recipient only. This is also provided by One-Time Pads. Let me know if there are other options, I'm curious.
- Authentication, or proving that a message has not been tampered with during transmission. This is [Rivest's](http://web.mit.edu/6.857/OldStuff/Fall97/lectures/lecture3.pdf) affine transform, covered under "Perfect Authentication".
- Secret sharing, which is an obscure crypto thing. This is Shamir's secret sharing scheme, which is based on polynomial interpolation over finite fields.

Problems that CANNOT be solved using perfect crypto:
- Key exchange, or agreeing on a key between two people who have never met before, so that an eavesdropper doesn't learn the secret key
- Commitment, or proving that you will reveal a specific message without yet revealing what it is

Problems where I'm not sure
- Public-key encryption, against passive eavesdropping
- Signing, or proving that a message is from who it says, to the general public
