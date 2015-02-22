/* GtkToolkit.java -- Implements an AWT Toolkit using GTK for peers
   Copyright (C) 1998, 1999, 2002, 2003, 2004, 2005  Free Software Foundation, Inc.

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


package gnu.java.awt.peer.gtk;

import gnu.classpath.Configuration;
import gnu.java.awt.EmbeddedWindow;
import gnu.java.awt.peer.ClasspathFontPeer;
import gnu.java.awt.peer.ClasspathTextLayoutPeer;
import gnu.java.awt.peer.EmbeddedWindowPeer;

import java.awt.*;
import java.awt.datatransfer.Clipboard;
import java.awt.dnd.DragGestureEvent;
import java.awt.dnd.peer.DragSourceContextPeer;
import java.awt.font.FontRenderContext;
import java.awt.im.InputMethodHighlight;
import java.awt.image.BufferedImage;
import java.awt.image.ColorModel;
import java.awt.image.DirectColorModel;
import java.awt.image.ImageConsumer;
import java.awt.image.ImageObserver;
import java.awt.image.ImageProducer;
import java.awt.peer.*;
import java.io.InputStream;
import java.net.URL;
import java.text.AttributedString;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Properties;

import javax.imageio.spi.IIORegistry;

/* This class uses a deprecated method java.awt.peer.ComponentPeer.getPeer().
   This merits comment.  We are basically calling Sun's bluff on this one.
   We think Sun has deprecated it simply to discourage its use as it is
   bad programming style.  However, we need to get at a component's peer in
   this class.  If getPeer() ever goes away, we can implement a hash table
   that will keep up with every window's peer, but for now this is faster. */

/**
 * This class accesses a system property called
 * <tt>gnu.java.awt.peer.gtk.Graphics</tt>.  If the property is defined and
 * equal to "Graphics2D", the cairo-based GdkGraphics2D will be used in
 * drawing contexts. Any other value will cause the older GdkGraphics
 * object to be used.
 */
public class GtkToolkit extends gnu.java.awt.ClasspathToolkit
{
  Hashtable containers = new Hashtable();
  static EventQueue q;
  static boolean useGraphics2dSet;
  static boolean useGraphics2d;
  static Thread mainThread;

  public static boolean useGraphics2D()
  {
    if (useGraphics2dSet)
      return useGraphics2d;
    useGraphics2d = System.getProperty("gnu.java.awt.peer.gtk.Graphics", 
                                       "Graphics").equals("Graphics2D");
    useGraphics2dSet = true;
    return useGraphics2d;
  }

  static native void gtkInit(int portableNativeSync);

  static
  {
    if (Configuration.INIT_LOAD_LIBRARY)
      System.loadLibrary("gtkpeer");

    int portableNativeSync;     
    String portNatSyncProp = 
      System.getProperty("gnu.classpath.awt.gtk.portable.native.sync");
    
    if (portNatSyncProp == null)
      portableNativeSync = -1;  // unset
    else if (Boolean.valueOf(portNatSyncProp).booleanValue())
      portableNativeSync = 1;   // true
    else
      portableNativeSync = 0;   // false

    gtkInit(portableNativeSync);

    mainThread = new Thread ("GTK main thread")
      {
        public void run ()
        {
          gtkMain ();
        }
      };
    mainThread.start ();
  }

  public GtkToolkit ()
  {
  }

  public native void beep();
  private native void getScreenSizeDimensions(int[] xy);
  
  public int checkImage (Image image, int width, int height, 
			 ImageObserver observer) 
  {
    int status = ImageObserver.ALLBITS 
      | ImageObserver.WIDTH 
      | ImageObserver.HEIGHT;

    if (image instanceof GtkImage)
	return ((GtkImage) image).checkImage (observer);

    if (observer != null)
      observer.imageUpdate (image, status,
                            -1, -1,
                            image.getWidth (observer),
                            image.getHeight (observer));
    
    return status;
  }

  /** 
   * A helper class to return to clients in cases where a BufferedImage is
   * desired but its construction fails.
   */
  private class GtkErrorImage extends Image
  {
    public GtkErrorImage()
    {
    }

    public int getWidth(ImageObserver observer)
    {
      return -1;
    }

    public int getHeight(ImageObserver observer)
    {
      return -1;
    }

    public ImageProducer getSource()
    {

      return new ImageProducer() 
        {          
          HashSet consumers = new HashSet();          
          public void addConsumer(ImageConsumer ic)
          {
            consumers.add(ic);
          }

          public boolean isConsumer(ImageConsumer ic)
          {
            return consumers.contains(ic);
          }

          public void removeConsumer(ImageConsumer ic)
          {
            consumers.remove(ic);
          }

          public void startProduction(ImageConsumer ic)
          {
            consumers.add(ic);
            Iterator i = consumers.iterator();
            while(i.hasNext())
              {
                ImageConsumer c = (ImageConsumer) i.next();
                c.imageComplete(ImageConsumer.IMAGEERROR);
              }
          }
          public void requestTopDownLeftRightResend(ImageConsumer ic)
          {
            startProduction(ic);
          }        
        };
    }

    public Graphics getGraphics() 
    { 
      return null; 
    }

    public Object getProperty(String name, ImageObserver observer)
    {
      return null;
    }
    public Image getScaledInstance(int width, int height, int flags)
    {
      return new GtkErrorImage();
    }

    public void flush() 
    {
    }
  }


  /** 
   * Helper to return either a BufferedImage -- the argument -- or a
   * GtkErrorImage if the argument is null.
   */

  private Image bufferedImageOrError(BufferedImage b)
  {
    if (b == null) 
      return new GtkErrorImage();
    else
      return b;
  }


  public Image createImage (String filename)
  {
    if (filename.length() == 0)
      return new GtkImage ();

    if (useGraphics2D())
      return bufferedImageOrError(GdkPixbufDecoder.createBufferedImage (filename));
    else
      return new GtkImage (filename);
  }

  public Image createImage (URL url)
  {
    if (useGraphics2D())
      return bufferedImageOrError(GdkPixbufDecoder.createBufferedImage (url));
    else
      return new GtkImage (url);
  }

  public Image createImage (ImageProducer producer) 
  {
    if (useGraphics2D())
      return bufferedImageOrError(GdkPixbufDecoder.createBufferedImage (producer));
    else
      return new GtkImage (producer);
  }

  public Image createImage (byte[] imagedata, int imageoffset,
			    int imagelength)
  {
    if (useGraphics2D())
      return bufferedImageOrError(GdkPixbufDecoder.createBufferedImage (imagedata,
                                                   imageoffset, 
                                                                        imagelength));
    else
      {
        byte[] datacopy = new byte[imagelength];
        System.arraycopy (imagedata, imageoffset, datacopy, 0, imagelength);
        return new GtkImage (datacopy);
      }
  }
  
  /**
   * Creates an ImageProducer from the specified URL. The image is assumed
   * to be in a recognised format. 
   *
   * @param url URL to read image data from.
   */  
  public ImageProducer createImageProducer(URL url)
  {
    return new GdkPixbufDecoder(url);  
  }

  /**
   * Returns the native color model (which isn't the same as the default
   * ARGB color model, but doesn't have to be). 
   */
  public ColorModel getColorModel () 
  {
    /* Return the GDK-native ABGR format */
    return new DirectColorModel(32, 
				0x000000FF,
				0x0000FF00,
				0x00FF0000,
				0xFF000000);
  }

  public String[] getFontList () 
  {
    return (new String[] { "Dialog", 
			   "DialogInput", 
			   "Monospaced", 
			   "Serif", 
			   "SansSerif" });
  }

  private class LRUCache extends LinkedHashMap
  {    
    int max_entries;
    public LRUCache(int max)
    {
      super(max, 0.75f, true);
      max_entries = max;
    }
    protected boolean removeEldestEntry(Map.Entry eldest)
    {
      return size() > max_entries;
    }
  }

  private LRUCache fontCache = new LRUCache(50);
  private LRUCache metricsCache = new LRUCache(50);
  private LRUCache imageCache = new LRUCache(50);

  public FontMetrics getFontMetrics (Font font) 
  {
    synchronized (metricsCache)
      {
        if (metricsCache.containsKey(font))
          return (FontMetrics) metricsCache.get(font);
      }

    FontMetrics m = new GdkFontMetrics (font);
    synchronized (metricsCache)
      {
        metricsCache.put(font, m);
      }
    return m;
  }

  public Image getImage (String filename) 
  {
    if (imageCache.containsKey(filename))
      return (Image) imageCache.get(filename);
    else
      {
        Image im = createImage(filename);
        imageCache.put(filename, im);
        return im;
      }
  }

  public Image getImage (URL url) 
  {
    if (imageCache.containsKey(url))
      return (Image) imageCache.get(url);
    else
      {
        Image im = createImage(url);
        imageCache.put(url, im);
        return im;
      }
  }

  public PrintJob getPrintJob (Frame frame, String jobtitle, Properties props) 
  {
    return null;
  }

  public native int getScreenResolution();

  public Dimension getScreenSize ()
  {
    int dim[] = new int[2];
    getScreenSizeDimensions(dim);
    return new Dimension(dim[0], dim[1]);
  }

  public Clipboard getSystemClipboard() 
  {
    SecurityManager secman = System.getSecurityManager();
    if (secman != null)
      secman.checkSystemClipboardAccess();

    return GtkClipboard.getInstance();
  }

  /**
   * Prepares a GtkImage. For every other kind of Image it just
   * assumes the image is already prepared for rendering.
   */
  public boolean prepareImage (Image image, int width, int height, 
			       ImageObserver observer) 
  {
    /* GtkImages are always prepared, as long as they're loaded. */
    if (image instanceof GtkImage)
      return ((((GtkImage)image).checkImage (observer) & 
	       ImageObserver.ALLBITS) != 0);

    /* Assume anything else is too */
    return true;
  }

  public native void sync();

  protected void setComponentState (Component c, GtkComponentPeer cp)
  {
    /* Make the Component reflect Peer defaults */
    if (c.getForeground () == null)
      c.setForeground (cp.getForeground ());
    if (c.getBackground () == null)
      c.setBackground (cp.getBackground ());
    //        if (c.getFont () == null)
    //  	c.setFont (cp.getFont ());
      
    /* Make the Peer reflect the state of the Component */
    if (! (c instanceof Window))
      {
	cp.setCursor (c.getCursor ());
	
	Rectangle bounds = c.getBounds ();
	cp.setBounds (bounds.x, bounds.y, bounds.width, bounds.height);
	cp.setVisible (c.isVisible ());
      }
  }

  protected ButtonPeer createButton (Button b)
  {
    return new GtkButtonPeer (b);
  }

  protected CanvasPeer createCanvas (Canvas c) 
  {
    return new GtkCanvasPeer (c);
  }

  protected CheckboxPeer createCheckbox (Checkbox cb) 
  {
    return new GtkCheckboxPeer (cb);
  }

  protected CheckboxMenuItemPeer createCheckboxMenuItem (CheckboxMenuItem cmi)
  {
    return new GtkCheckboxMenuItemPeer (cmi);
  }

  protected ChoicePeer createChoice (Choice c) 
  {
    return new GtkChoicePeer (c);
  }

  protected DialogPeer createDialog (Dialog d)
  {
    return new GtkDialogPeer (d);
  }

  protected FileDialogPeer createFileDialog (FileDialog fd)
  {
    return new GtkFileDialogPeer (fd);
  }

  protected FramePeer createFrame (Frame f)
  {
    return new GtkFramePeer (f);
  }

  protected LabelPeer createLabel (Label label) 
  {
    return new GtkLabelPeer (label);
  }

  protected ListPeer createList (List list)
  {
    return new GtkListPeer (list);
  }

  protected MenuPeer createMenu (Menu m) 
  {
    return new GtkMenuPeer (m);
  }

  protected MenuBarPeer createMenuBar (MenuBar mb) 
  {
    return new GtkMenuBarPeer (mb);
  }

  protected MenuItemPeer createMenuItem (MenuItem mi) 
  {
    return new GtkMenuItemPeer (mi);
  }

  protected PanelPeer createPanel (Panel p) 
  {
    return new GtkPanelPeer (p);
  }

  protected PopupMenuPeer createPopupMenu (PopupMenu target) 
  {
    return new GtkPopupMenuPeer (target);
  }

  protected ScrollPanePeer createScrollPane (ScrollPane sp) 
  {
    return new GtkScrollPanePeer (sp);
  }

  protected ScrollbarPeer createScrollbar (Scrollbar sb) 
  {
    return new GtkScrollbarPeer (sb);
  }

  protected TextAreaPeer createTextArea (TextArea ta) 
  {
    return new GtkTextAreaPeer (ta);
  }

  protected TextFieldPeer createTextField (TextField tf) 
  {
    return new GtkTextFieldPeer (tf);
  }

  protected WindowPeer createWindow (Window w)
  {
    return new GtkWindowPeer (w);
  }

  public EmbeddedWindowPeer createEmbeddedWindow (EmbeddedWindow w)
  {
    return new GtkEmbeddedWindowPeer (w);
  }

  /** 
   * @deprecated part of the older "logical font" system in earlier AWT
   * implementations. Our newer Font class uses getClasspathFontPeer.
   */
  protected FontPeer getFontPeer (String name, int style) {
    // All fonts get a default size of 12 if size is not specified.
    return getFontPeer(name, style, 12);
  }

  /**
   * Private method that allows size to be set at initialization time.
   */
  private FontPeer getFontPeer (String name, int style, int size) 
  {
    Map attrs = new HashMap ();
    ClasspathFontPeer.copyStyleToAttrs (style, attrs);
    ClasspathFontPeer.copySizeToAttrs (size, attrs);
    return getClasspathFontPeer (name, attrs);
  }

  /**
   * Newer method to produce a peer for a Font object, even though Sun's
   * design claims Font should now be peerless, we do not agree with this
   * model, hence "ClasspathFontPeer". 
   */

  public ClasspathFontPeer getClasspathFontPeer (String name, Map attrs)
  {
    Map keyMap = new HashMap (attrs);
    // We don't know what kind of "name" the user requested (logical, face,
    // family), and we don't actually *need* to know here. The worst case
    // involves failure to consolidate fonts with the same backend in our
    // cache. This is harmless.
    keyMap.put ("GtkToolkit.RequestedFontName", name);
    if (fontCache.containsKey (keyMap))
      return (ClasspathFontPeer) fontCache.get (keyMap);
    else
      {
        ClasspathFontPeer newPeer = new GdkFontPeer (name, attrs);
        fontCache.put (keyMap, newPeer);
        return newPeer;
      }
  }

  public ClasspathTextLayoutPeer getClasspathTextLayoutPeer (AttributedString str, 
                                                             FontRenderContext frc)
  {
    return new GdkTextLayout(str, frc);
  }

  protected EventQueue getSystemEventQueueImpl() 
  {
    synchronized (GtkToolkit.class)
      {
        if (q == null)
          {
            q = new EventQueue();
            GtkGenericPeer.enableQueue (q);
          }
      }    
    return q;
  }

  protected native void loadSystemColors (int[] systemColors);

  public DragSourceContextPeer createDragSourceContextPeer(DragGestureEvent e)
  {
    throw new Error("not implemented");
  }

  public Map mapInputMethodHighlight(InputMethodHighlight highlight)
  {
    throw new Error("not implemented");
  }

  public Rectangle getBounds()
  {
    int[] dims = new int[2];
    getScreenSizeDimensions(dims);
    return new Rectangle(0, 0, dims[0], dims[1]);
  }
  
  // ClasspathToolkit methods

  public GraphicsEnvironment getLocalGraphicsEnvironment()
  {
    return new GdkGraphicsEnvironment();
  }

  public Font createFont(int format, InputStream stream)
  {
    throw new UnsupportedOperationException();
  }

  public RobotPeer createRobot (GraphicsDevice screen) throws AWTException
  {
    return new GdkRobotPeer (screen);
  }

  public void registerImageIOSpis(IIORegistry reg)
  {
    GdkPixbufDecoder.registerSpis(reg);
  }

  public static native void gtkMain();
} // class GtkToolkit
