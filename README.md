# XOTP : XOR One Time Pad v1.02

> "The one-time pad (OTP) is an encryption technique that cannot be cracked, but requires the use of a one-time pre-shared key the same size as, or longer than, the message being sent. In this technique, a plaintext is paired with a random secret key (also referred to as a one-time pad). Then, each bit or character of the plaintext is encrypted by combining it with the corresponding bit or character from the pad. "

[(Wikipedia)](https://en.wikipedia.org/wiki/One-time_pad)

XOTP is an implementation of this , written in C, that uses the bitwise [Exclusive-OR (XOR)](https://en.wikipedia.org/wiki/Exclusive_or) to combine the bytes of the plaintext with those of the key. 

### Important !
XOTP is alpha. At the moment it fully buffers the file , pad and ciphertext. This is a *known* problem, and a fix is coming *soon* in the form of a **complete rewrite** in v2.

### Usage
Once you have downloaded from releases or compiled from source , the below will help you use XOTP.

XOTP takes commands in the format : `xotp mode sourcefile keyfile` . 

The mode is `e` or `d` stand for encryption or decryption. So to decrypt one would use `xotp d sourcefile keyfile`.

When encrypting, the `keyfile` argument is *optional*, and if left blank, XOTP automatically generates a key and saves it as `sourcefile.key`. You need only specify a `keyfile` while encrypting  if you intentionally wish to use a *specific , pre-determined* keyfile. If you do so, [do not reuse it](https://crypto.stackexchange.com/a/108/82847).

XOTP outputs the encrypted file as `sourcefile.xotp` so `hello.txt` becomes `hello.txt.xotp` , just like with [GPG](https://en.wikipedia.org/wiki/GNU_Privacy_Guard). Hence, while decrypting, the *last five* letters are **removed** from the filename of the file entered. All such defaults can be easily changed by changing the values of the macros in the source code and re-compiling. 

### Compiling
Compiling is not necessary, you can always download from the release. If your OS is not supported, the released binary does not work for you, you want to modify the source code, or some other reason, the below will help you compile XOTP yourself.

- **Windows :**
1. [Download](https://minhaskamal.github.io/DownGit/#/home?url=https:%2F%2Fgithub.com%2Fa-p-jo%2FXOTP%2Ftree%2Fmain%2FSource) the source files, if zipped , unzip them.
2. Download and install [Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2019) or [Visual Studio](https://visualstudio.microsoft.com/downloads/#visual-studio-community-2019). Build tools are smaller and have everything we will need here, so it is suggested to use them. If running anything older than Windows 7 SP1, get [Visual Studio 2010](https://docs.microsoft.com/en-gb/visualstudio/releasenotes/vs2010-version-history) or older, which has not been tested but *should work*, but steps to compile will likely differ from those given below.
3. Search for in start menu and run "`x64 Native Tools Command Prompt...`". Use "`Developer Command Prompt...`" if compiling for 32-bit Windows.
4. Use `cd` in the command prompt to navigate to the folder contaning the source files. Example : `cd c:\Users\YOUR_USERNAME\Downloads\Source`
5. Do : `cl main.c /link /out:xotp.exe`. If successful , this wil compile the source files to create `xotp.exe` in the file containing the source.

You can now execute XOTP from the [current directory](https://en.wikipedia.org/wiki/Working_directory) with `xotp` or from elsewhere with `c:\...\xotp` where `c:\...` is the [absolute file path](https://en.wikipedia.org/wiki/Path_%28computing%29#Absolute_and_relative_paths) to XOTP. 

- [**UNIX-like OS :**](https://en.wikipedia.org/wiki/Unix-like)

Note : On macOS , compiling needs [xcode utilities](https://developer.apple.com/library/archive/technotes/tn2339/_index.html), installed with `xcode-select --install`.

In a terminal, Do : 

1. `curl -O https://raw.githubusercontent.com/a-p-jo/XOTP/main/Source/main.c` 
or `wget https://raw.githubusercontent.com/a-p-jo/XOTP/main/Source/main.c`
2. `curl -O https://raw.githubusercontent.com/a-p-jo/XOTP/main/Source/otp.h` 
or `wget https://raw.githubusercontent.com/a-p-jo/XOTP/main/Source/otp.h`
3. `cc main.c -o xotp` 

You can now execute XOTP from the [current directory](https://en.wikipedia.org/wiki/Working_directory) with `./xotp` or from elsewhere with `/.../xotp` where `/...` is the [absolute file path](https://en.wikipedia.org/wiki/Path_%28computing%29#Absolute_and_relative_paths) to XOTP. 

### Advantages and Disadvantages
- The advantage is that given truly random bytes in the key, and the key kept secret and never reused, it is truly ***impossible*** to break even with *infinite* compute power and also offers *perfect secrecy*, i.e., looking at the encrypted file says nothing about the original file beyond file size.

- The disadvantage is the large storage space used, which may also make secure key transport hard. This is because the pad/key has to be as big as the file . It can be relatively slower due to slowness of random generation and [I/O intensiveness](https://en.wikipedia.org/wiki/I/O_bound), making it less practical for larger file sizes. 

### Conditions and Security :
- True randomness is critical . On a UNIX-like system (Linux, macOS, BSD...) , this program uses `/dev/urandom` to generate the pad , which is adequate, and although `/dev/random` is arguable *more* "truly random", `/dev/urandom` is [not *bad*](https://www.2uo.de/myths-about-urandom/). On Windows, it uses `rand_s()` , which Microsoft states to have cryptographically secure randomness. 

On a system that is neither , XOTP defaults to the C standard library's `rand()` seeded with current time `time(NULL)`. This is not a secure arrangement, but it does cover portability as well as convineance, and it is assumed that not much critical encryption will be done with a One Time Pad in an obscure OS anyway. You can ***always*** generate your own pads (say with `dd` and `/dev/random`) and use them if you wish : XOTP  can use any file you tell it to as a pad , provided it is at least as large as the file you wish to encrypt. Do not reuse the pad.

- Security of keys in transit is critical. The keys are exactly as large as the file they encrypt, and this means you will have to be able to share twice the amount of data as the size of the file. If this can be securely transferred, brilliant. But do not use it if that is not possible; it is no different from leaving the key in the physical lock when leaving the house; use different encryption.

-  Enough available (3x size of file) RAM and Disk space is necessary to encrypt and decrypt.

### How it Works
A simplification of the tasks carried out by XOPT is :

1. XOTP determines the size of the file in bytes given to encrypt, by calling `ftell()` after moving to the end of the file.

2. The entire file is buffered in memory in an array of bytes (`unint8_t`). This type of array allows access to each byte and buffering makes it much faster than querying from disk byte-by-byte. *This process is inefficient in memory usage and must be changed.*

3. Then, depending on whether encryption or decryption is required, a global variable is assigned a corresponding value.

4. If encrypting and a keyfile is given, it is buffered to memory.*This process is inefficient in memory usage and must be changed.* Otherwise a key buffer as big as the file composed of random bytes is generated and saved to a file with the given file's filename + a suffix , by default `".key"`. This generation is performed using `/dev/urandom` in UNIX-like systems , `rand_s()` in Windows and `stdlib`'s `srand()` and `rand()` in any other platform. If decrypting, the keyfile is necessary and is likewise buffered. 

5. Another buffer is generated consisting of the XOR of all the bytes from the file and the key buffers. *This process is inefficient in memory usage and must be changed.* This buffer is saved either with the suffix `".xotp"` added to the original filename (if encrypting) or with the last 4 characters of the filename removed (if decrypting). This is thus either the ciphertext or the plaintext !

There is other complexity , but those are details of implementation, such as memory-management , error-checking, file-stream management, etc. The core idea and functionality really is this simple and the code is < 400 lines (and can be much simpler once buffering is tamed to be effecient).

###### I hope you find joy and purpose in using or modifying this Free software just as I did developing it.
