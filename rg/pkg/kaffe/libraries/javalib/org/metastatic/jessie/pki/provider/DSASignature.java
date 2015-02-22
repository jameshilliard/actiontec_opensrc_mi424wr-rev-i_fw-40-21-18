/* DSASignature.java
   Copyright (C) 1999,2003 Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version. */


package org.metastatic.jessie.pki.provider;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;

import java.math.BigInteger;

import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.InvalidParameterException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.NoSuchProviderException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.SignatureException;
import java.security.SignatureSpi;
import java.security.interfaces.DSAPrivateKey;
import java.security.interfaces.DSAPublicKey;
import java.security.spec.AlgorithmParameterSpec;

import java.util.ArrayList;
import java.util.List;
import java.util.Random;

import org.metastatic.jessie.pki.io.ASN1ParsingException;
import org.metastatic.jessie.pki.der.DER;
import org.metastatic.jessie.pki.der.DEREncodingException;
import org.metastatic.jessie.pki.der.DERReader;
import org.metastatic.jessie.pki.der.DERValue;
import org.metastatic.jessie.pki.der.DERWriter;

public class DSASignature extends SignatureSpi
{
  private DSAPublicKey publicKey;
  private DSAPrivateKey privateKey;
  private MessageDigest digest = null;

  public DSASignature()
  {
  }

  private void init()
  {
    if (digest == null)
      {
        try
          {
            digest = MessageDigest.getInstance("SHA1", "GNU-PKI");
          }
        catch (NoSuchAlgorithmException nsae)
          {
            throw new Error(nsae);
          }
        catch (NoSuchProviderException nspe)
          {
            throw new Error(nspe);
          }
      }
    digest.reset();
  }

  public void engineInitVerify(PublicKey publicKey) throws InvalidKeyException
  {
    if(publicKey instanceof DSAPublicKey)
      {
        this.publicKey = (DSAPublicKey) publicKey;
        privateKey = null;
      }
    else
      throw new InvalidKeyException();
    init();
  }

  public void engineInitSign(PrivateKey privateKey) throws InvalidKeyException
  {
    if (privateKey instanceof DSAPrivateKey)
      {
        this.privateKey = (DSAPrivateKey) privateKey;
        publicKey = null;
      }
    else
      throw new InvalidKeyException();

    init();
  }

  public void engineInitSign(PrivateKey privateKey, SecureRandom random)
    throws InvalidKeyException
  {
    if (privateKey instanceof DSAPrivateKey)
      {
        this.privateKey = (DSAPrivateKey)privateKey;
        publicKey = null;
      }
    else
      throw new InvalidKeyException();

    appRandom = random;
    init();
  }

  public void engineUpdate(byte b) throws SignatureException
  {
    if (digest == null)
      throw new SignatureException();

    digest.update(b);
  }

  public void engineUpdate(byte[] b, int off, int len)
    throws SignatureException
  {
    if (digest == null)
      throw new SignatureException();

    digest.update(b, off, len);
  }

  public byte[] engineSign() throws SignatureException
  {
    if (digest == null)
      throw new SignatureException();
    if (privateKey == null)
      throw new SignatureException();

    try
      {
        BigInteger g = privateKey.getParams().getG();
        BigInteger p = privateKey.getParams().getP();
        BigInteger q = privateKey.getParams().getQ();

        BigInteger x = privateKey.getX();

        BigInteger k = new BigInteger(159, (Random) appRandom);

        BigInteger r = g.modPow(k, p);
        r = r.mod(q);

        byte bytes[] = digest.digest();
        BigInteger sha = new BigInteger(1, bytes);

        BigInteger s = sha.add(x.multiply( r ));
        s = s.multiply(k.modInverse(q)).mod(q);

        ByteArrayOutputStream bout = new ByteArrayOutputStream();
        ArrayList seq = new ArrayList(2);
        seq.add(new DERValue(DER.INTEGER, r));
        seq.add(new DERValue(DER.INTEGER, s));
        DERWriter.write(bout, new DERValue(DER.CONSTRUCTED | DER.SEQUENCE, seq));
        return bout.toByteArray();
      }
    catch (IOException ioe)
      {
        throw new SignatureException();
      }
    catch (ArithmeticException ae)
      {
        throw new SignatureException();
      }
  }

  public int engineSign(byte[] outbuf, int offset, int len)
    throws SignatureException
  {
    byte tmp[] = engineSign();
    if(tmp.length > len)
      throw new SignatureException();
    System.arraycopy(tmp, 0, outbuf, offset, tmp.length);
    return tmp.length;
  }

  public boolean engineVerify(byte[] sigBytes) throws SignatureException
  {
    // Decode sigBytes from ASN.1 DER encoding
    try
      {
        DERReader in = new DERReader(sigBytes);
        DERValue val = in.read();
        if (!val.isConstructed())
          throw new SignatureException("badly formed signature");
        BigInteger r = (BigInteger) in.read().getValue();
        BigInteger s = (BigInteger) in.read().getValue();

        BigInteger g = publicKey.getParams().getG();
        BigInteger p = publicKey.getParams().getP();
        BigInteger q = publicKey.getParams().getQ();

        BigInteger y = publicKey.getY();

        BigInteger w = s.modInverse(q);

        byte bytes[] = digest.digest();
        BigInteger sha = new BigInteger(1, bytes);

        BigInteger u1 = w.multiply(sha).mod(q);
        BigInteger u2 = r.multiply(w).mod(q);

        BigInteger v = g.modPow(u1, p).multiply(y.modPow( u2, p )).mod(p).mod(q);

        return v.equals(r);
      }
    catch (IOException ioe)
      {
        throw new SignatureException("badly formed signature");
      }
  }

  public void engineSetParameter(String param, Object value)
    throws InvalidParameterException
  {
    throw new InvalidParameterException();
  }

  public void engineSetParameter(AlgorithmParameterSpec params)
    throws InvalidAlgorithmParameterException
  {
    throw new InvalidParameterException();

  }

  public Object engineGetParameter(String param)
    throws InvalidParameterException
  {
    throw new InvalidParameterException();
  }

  public Object clone() throws CloneNotSupportedException
  {
    return new DSASignature(this);
  }

  private DSASignature(DSASignature copy) throws CloneNotSupportedException
  {
    this();
    this.publicKey = copy.publicKey;
    this.privateKey = copy.privateKey;
    if (copy.digest != null)
      this.digest = (MessageDigest) copy.digest.clone();
  }
}
