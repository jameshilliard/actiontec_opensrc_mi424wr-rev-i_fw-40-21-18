package gnu.crypto.jce.mac;

// --------------------------------------------------------------------------
// $Id: UMac32Spi.java,v 1.1.1.1 2007/05/07 23:32:39 jungo Exp $
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
import gnu.crypto.mac.UMac32;
import gnu.crypto.jce.spec.UMac32ParameterSpec;

import java.security.InvalidAlgorithmParameterException;
import java.security.InvalidKeyException;
import java.security.Key;
import java.security.spec.AlgorithmParameterSpec;

/**
 * The implementation of the UMAC-32 <i>Service Provider Interface</i>
 * (<b>SPI</b>) adapter.
 *
 * @version $Revision: 1.1.1.1 $
 */
public final class UMac32Spi extends MacAdapter {

   // Constructors.
   // -----------------------------------------------------------------------

   public UMac32Spi() {
      super(Registry.UMAC32);
   }

   // Instance methods overriding MacAdapter.
   // -----------------------------------------------------------------------

   protected void engineInit(Key key, AlgorithmParameterSpec params)
   throws InvalidKeyException, InvalidAlgorithmParameterException
   {
      if (!(params instanceof UMac32ParameterSpec)) {
         throw new InvalidAlgorithmParameterException();
      }
      if (params != null) {
         attributes.put(UMac32.NONCE_MATERIAL,
            ((UMac32ParameterSpec) params).getNonce());
      }
      try {
         super.engineInit(key, null);
      } catch (IllegalArgumentException iae) {
         throw new InvalidAlgorithmParameterException(iae.getMessage());
      }
   }
}
