#!/usr/bin/python
import sys
import binascii

from hashlib import sha256

from base58 import b58encode, b58decode

ABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"

class BurnQSTEESError(Exception):
  pass

class AlphabetError(BurnQSTEESError):
  pass

def hh256(s):
  s = sha256(s).digest()
  return binascii.hexlify(sha256(s).digest())

def b58ec(s):
  unencoded = str(bytearray.fromhex(unicode(s)))
  encoded = b58encode(unencoded)
  return encoded

def b58dc(encoded, trim=0):
  unencoded = b58decode(encoded)[:-trim]
  return unencoded

def burn(s):
  decoded = b58dc(s, trim=4)
  decoded_hex = binascii.hexlify(decoded)
  check = hh256(decoded)[:8]
  coded = decoded_hex + check
  return b58ec(coded)

def usage():
  print "usage: python BurnAddress TEMPLATE"
  print
  print "   TEMPLATE - 34 letters & numbers (no zeros)"
  print "              the first two are coin specific"
  raise SystemExit

if __name__ == "__main__":
  if len(sys.argv) != 2:
    usage()
  if sys.argv[1] == "test":
    template = "QsteesBurnAddressForPubContentXXXXXXX"
  else:
    template = sys.argv[1]
  for c in template:
    if c not in ABET:
      raise AlphabetError("Character '%s' is not valid base58." % c)
    tlen = len(template)
    if tlen < 34:
      template = template + ((34 - tlen) * "X")
    else:
      template = template[:34]
  print burn(template)