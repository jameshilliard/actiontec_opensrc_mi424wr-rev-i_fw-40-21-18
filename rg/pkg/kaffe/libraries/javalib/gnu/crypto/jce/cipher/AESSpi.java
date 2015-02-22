package gnu.crypto.jce.cipher;

// --------------------------------------------------------------------------
// $Id: AESSpi.java,v 1.1.1.1 2007/05/07 23:32:39 jungo Exp $
//
// Copyright (C) 2002 Free Software Foundation, Inc.
//
// This file is part of GNU Crypto.
//
// GNU Crypto is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2 of the License, or (at
// your option) any later version.
//
// GNU Crypto is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the
//
//    Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor,
//    Boston, MA  02110-1301
//    USA
//
// Linking this library statically or dynamically with other modules is
// making a combined work based on this library.  Thus, the terms and
// conditions of the GNU General Public License cover the whole
// combination.
//
// As a special exception, the copyright holders of this library give
// you permission to link this library with independent modules to
// produce an executable, regardless of the license terms of these
// independent modules, and to copy and distribute the resulting
// executable under terms of your choice, provided that you also meet,
// for each linked independent module, the terms and conditions of the
// license of that module.  An independent module is a module which is
// not derived from or based on this library.  If you modify this
// library, you may extend this exception to your version of the
// library, but you are not obligated to do so.  If you do not wish to
// do so, delete this exception statement from your version.
//
// --------------------------------------------------------------------------

import gnu.crypto.Registry;
import gnu.crypto.jce.spec.BlockCipherParameterSpec;

import java.security.AlgorithmParameters;
import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.SecureRandom;
import java.security.spec.AlgorithmParameterSpec;
import java.security.spec.InvalidParameterSpecException;

/**
 * The implementation of the AES <i>Service Provider Interface</i>
 * (<b>SPI</b>) adapter.
 *
 * @version $Revision: 1.1.1.1 $
 */
public final class AESSpi extends CipherAdapter {

   // Constructors.
   // -----------------------------------------------------------------------

   public AESSpi() {
      super(Registry.AES_CIPHER, 16);
   }

   // Methods from CipherAdapter
   // -----------------------------------------------------------------------

   protected void engineInit(int opmode, Key key,
      AlgorithmParameterSpec params, SecureRandom random)
   throws InvalidKeyException, InvalidAlgorithmParameterException
   {
      if (params instanceof BlockCipherParameterSpec) {
         if (((BlockCipherParameterSpec) params).getBlockSize() != 16) {
            throw new InvalidAlgorithmParameterException(
               "AES block size must be 16 bytes");
         }
      }
      super.engineInit(opmode, key, params, random);
   }

   protected void engineInit(int opmode, Key key,
      AlgorithmParameters params, SecureRandom random)
   throws InvalidKeyException, InvalidAlgorithmParameterException
   {
      AlgorithmParameterSpec spec = null;
      try {
         if (params != null) {
            spec = params.getParameterSpec(BlockCipherParameterSpec.class);
         }
      } catch (InvalidParameterSpecException ipse) { }
      engineInit(opmode, key, spec, random);
   }
}
