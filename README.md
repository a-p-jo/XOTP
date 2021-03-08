# XOTP : XOR One Time Pad v2.02

> "The one-time pad (OTP) is an encryption technique that cannot be cracked, but requires the use of a one-time pre-shared key the same size as, or longer than, the message being sent. In this technique, a plaintext is paired with a random secret key (also referred to as a one-time pad). Then, each bit or character of the plaintext is encrypted by combining it with the corresponding bit or character from the pad. "

[(Wikipedia)](https://en.wikipedia.org/wiki/One-time_pad)

XOTP is an implementation of this , written in C, that uses the bitwise [Exclusive-OR (XOR)](https://en.wikipedia.org/wiki/Exclusive_or) to combine the bytes of the plaintext with those of the key, hence the name.

### Usage 

As XOTP itself will tel you :

```
-> xotp [command : e or d] [(optional) options] [(optional) option's argument] ...

Options :

	1. "-f" : Give input file.
	 Without "-f", input is read from stdin.

	2. "-p" : Give pad file.
		2.1 If encrypting : if the pad file exists, it is read as the pad.
		If not, the generated pad is saved to it.
		Without "-p" , pad is saved to the input file (if specified) with a ".pad" extension.
		Otherwise, it is saved to "pad.pad"
		2.2 If decrypting : the pad file is read to decrypt the input.
		Without "-p", if "-f" is supplied, pad is read from stdin.

	3. "-o" : Give output file.
	Without "-o", output is written to stdout
```
The *idea* behind arguments to XOTP is "*control by implication*".
Explicit commands are also possible; the idea is to equip the user to imply more than he types.

So, invocations can be as small as `xotp e` . This implies that xotp will read from stdin , which it will encrypt by generating a pad of appropriate size saved to `pad.pad` (in the cwd) , and the encrypted data will be written to stdout.

Or, invokations can be as verbose as `xotp e -f foo -p pad -o bar`. Here, "`foo`" will be encrypted to "`bar`", with the pad "`pad`". If "`pad`" already exists, XOTP attempts to read it as the pad, if it does not, it generates a pad and saves it to "`pad`".

However, it is still kept simple : there are only 3 options , they can occur in any order, most of them are optional.

###### I hope you find joy and purpose in using or modifying this Free software just as I did developing it.
