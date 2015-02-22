package gnu.crypto.sasl.anonymous;

// ----------------------------------------------------------------------------
// $Id: AnonymousClient.java,v 1.1.1.1 2007/05/07 23:32:39 jungo Exp $
//
// Copyright (C) 2003 Free Software Foundation, Inc.
//
// This file is part of GNU Crypto.
//
// GNU Crypto is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// GNU Crypto is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; see the file COPYING.  If not, write to the
//
//    Free Software Foundation Inc.,
//    51 Franklin Street, Fifth Floor,
//    Boston, MA 02110-1301
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
// ----------------------------------------------------------------------------

import gnu.crypto.Registry;
import gnu.crypto.sasl.ClientMechanism;
import gnu.crypto.sasl.IllegalMechanismStateException;

import java.io.UnsupportedEncodingException;

import javax.security.sasl.SaslClient;
import javax.security.sasl.SaslException;
import javax.security.sasl.AuthenticationException;

/**
 * <p>The ANONYMOUS client-side mechanism.</p>
 *
 * @version $Revision: 1.1.1.1 $
 */
public class AnonymousClient extends ClientMechanism implements SaslClient {

   // Constants and variables
   // -------------------------------------------------------------------------

   // Constructor(s)
   // -------------------------------------------------------------------------

   public AnonymousClient() {
      super(Registry.SASL_ANONYMOUS_MECHANISM);
   }

   // Class methods
   // -------------------------------------------------------------------------

   // Instance methods
   // -------------------------------------------------------------------------

   // abstract methods implementation -----------------------------------------

   protected void initMechanism() throws SaslException {
   }

   protected void resetMechanism() throws SaslException {
   }

   // javax.security.sasl.SaslClient interface implementation -----------------

   public boolean hasInitialResponse() {
      return true;
   }

   public byte[] evaluateChallenge(final byte[] challenge) throws SaslException {
      if (complete) {
         throw new IllegalMechanismStateException("evaluateChallenge()");
      }
      return response();
   }

   private byte[] response() throws SaslException {
      if (!AnonymousUtil.isValidTraceInformation(authorizationID)) {
         throw new AuthenticationException(
               "Authorisation ID is not a valid email address");
      }
      complete = true;
//      return authorizationID.getBytes();
      final byte[] result;
      try {
         result = authorizationID.getBytes("UTF-8");
      } catch (UnsupportedEncodingException x) {
         throw new AuthenticationException("response()", x);
      }
      return result;
   }
}
