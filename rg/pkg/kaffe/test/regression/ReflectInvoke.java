import java.lang.*;
import java.lang.reflect.*;

/*
 * a very rudimentary test for Method.invoke()
 */
public class ReflectInvoke {

  static interface testInterface {
    void a ();
  }

  static class testImplBase implements testInterface {
    public void a () {
      System.out.println ("Success_Super");
    }
  }

  static class testImplSub extends testImplBase {
    public void a () {
      System.out.println ("Success_Sub");
    }
  }
 
  public int dd(int i)	{ return 2 * i; }
  public static int DD(int i)	{ return i/2; }

  public static void main(String[] argv)
  {
    try {
      Class c = ReflectInvoke.class;

      Class argtypes[] = new Class[1];
      argtypes[0] = int.class;
      Object args[] = new Object[1];
      args[0] = new Integer(4);
      Object o = c.getMethod("DD", argtypes).invoke(null, args);
      System.out.println(((Integer)o).intValue());

      o = c.getMethod("dd", argtypes).invoke(c.newInstance(), args);
      System.out.println(((Integer)o).intValue());

      Method m = testInterface.class.getMethod ("a", null);

      m.invoke (new testImplBase(), null);
      
      m.invoke (new testImplSub(), null);

      m = testImplBase.class.getMethod ("a", null);
      
      m.invoke (new testImplSub(), null);
    } catch (Exception e) {
      System.out.println("caught " + e);
      e.printStackTrace(System.out);
    }
  }
}

/* Expected Output:
2
8
Success_Super
Success_Sub
Success_Sub
*/
