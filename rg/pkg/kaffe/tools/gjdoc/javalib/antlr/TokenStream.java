package antlr;

/* ANTLR Translator Generator
 * Project led by Terence Parr at http://www.jGuru.com
 * Software rights: http://www.antlr.org/license.html
 *
 * $Id: TokenStream.java,v 1.1.1.1 2007/05/07 23:32:29 jungo Exp $
 */

public interface TokenStream {
    public Token nextToken() throws TokenStreamException;
}
