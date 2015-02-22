/* gnu.classpath.tools.gjdoc.SeeTagImpl
   Copyright (C) 2001 Free Software Foundation, Inc.

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
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA. */

package gnu.classpath.tools.gjdoc;

import com.sun.javadoc.*;
import java.util.*;
import java.text.*;

public class SeeTagImpl extends AbstractTagImpl implements SeeTag {

   protected String     reference;
   private String       referencedClassName;
   private String       referencedMemberName;
   private ClassDoc     referencedClass;
   private MemberDoc    referencedMember;
   private PackageDoc   referencedPackage;
   private String       label;
   private ClassDocImpl contextClass;

   public SeeTagImpl(String text, ClassDocImpl contextClass) {
      super(text);
      this.contextClass=contextClass;
   }

   public void resolve() {

      super.resolve();

      text = text.trim();

      if (text.startsWith("<") || text.startsWith("\"")) {
         label = text;
         return;
      }

      int labelNdx=text.indexOf(';');
      if (labelNdx>=0) {
	 label="";
	 return;
      }

      for (int i=0; i<text.length(); ++i) {
         if (" \t\r\n".indexOf(text.charAt(i)) >= 0) {
            labelNdx = i;
            break;
         }
      }

      int openParenNdx = text.indexOf('(');
      if (openParenNdx >= 0 && openParenNdx < labelNdx) {
         labelNdx=text.indexOf(')', openParenNdx);
         if (labelNdx >= 0) {
            ++ labelNdx;
         }
      }

      if (labelNdx<0 || labelNdx>=text.length()) {
	 reference=text.trim();
	 label="";
      }
      else {
	 reference=text.substring(0,labelNdx).trim();
	 label=text.substring(labelNdx).trim();
      }  

      int mspecNdx=reference.indexOf('#');
      String referencedFqName;
      if (mspecNdx<0) {
	 referencedFqName=reference;
      }
      else {
	 referencedFqName=reference.substring(0,mspecNdx);
	 referencedMemberName=reference.substring(mspecNdx+1); 
     }

      // the following is in contradiction to the api docs, but
      // conform to sun javadoc: return fully qualified classname
      // with referencedClassName().
      if (referencedFqName.trim().length()>0) {
	 referencedClassName=referencedFqName;
	 if (contextClass==null)
	    referencedClass=Main.getRootDoc().classNamed(referencedFqName);
	 else
	    referencedClass=contextClass.findClass(referencedFqName);
      }
      else {
	 referencedClassName="";
	 referencedClass=contextClass;
      }

      if (referencedClass==null) {
         referencedClass = Main.getRootDoc().classNamed("java.lang." + referencedFqName);
      }

      if (referencedClass!=null && !referencedClass.isIncluded()) referencedClass=null;

      if (referencedClass!=null) {
	 referencedPackage=referencedClass.containingPackage();
	 referencedClassName=referencedClass.qualifiedName();

	 if (referencedMemberName!=null) {
	    if (referencedMemberName.indexOf('(')<0) {
	       referencedMember=((ClassDocImpl)referencedClass).findField(referencedMemberName);
               if (null == referencedMember) {
                  MethodDoc[] methods = ((ClassDocImpl)referencedClass).methods();
                  for (int i=0; i<methods.length; ++i) {
                     if (methods[i].name().equals(referencedMemberName)) {
                        if (null == referencedMember) {
                           referencedMember = methods[i];
                        }
                        else {
                           referencedClass = null;
                           referencedMember = null;
                           //print warning here
                           break;
                        }
                     }
                  }
               }
	    }
	    else {
	       referencedMember=((ClassDocImpl)referencedClass).findExecutableRec(referencedMemberName);
               if (referencedMember==null) {
                  //System.err.println("cannot find member for '"+referencedMemberName+"'");
                  referencedClass = null;
               }
	    }
	 }
      }
      /*
      else {
	 System.err.println("class not found: '"+referencedFqName + "' in context class " + contextClass + " in " + this);
      }
      */
   }

   public ClassDoc referencedClass() {
      return referencedClass;
   }

   public String referencedClassName() {
      return referencedClassName;
   }

   public MemberDoc referencedMember() {
      return referencedMember;
   }

   public String referencedMemberName() {
      return referencedMemberName;
   }

   public PackageDoc referencedPackage() {
      return referencedPackage;
   }

   public String label() {
      return label;
   }

   public String kind() {
      return "@see";
   }

   public String name() {
      return "@see";
   }

   public Tag[] firstSentenceTags() {
      return inlineTags();
   }

   public Tag[] inlineTags() {
      return new Tag[]{new TextTagImpl(referencedClassName)};
   }
}
