/*
 * The Art of Reflecting Interfaces.
 */
import java.lang.reflect.Method;

public class ReflectInterfaces
{
	interface If1
	{
		public void if1Method();
		void if1MethodNP();
		public void if1MethodTH() throws Throwable;
	}

	interface If2 extends If1
	{
		public void if2Method();
		void if2MethodNP();
		public void if2MethodTH() throws Throwable;
	}

	abstract class C1 implements If2
	{
	}

	public static void main(String av[])
	{
		Class c = If2.class;
		Class sc = c.getSuperclass();
		System.out.println((sc == null?"null":sc.getName()));
		System.out.println("all methods");
		Method [] m = c.getMethods();
		for (int i = 0; i < m.length; i++)
			System.out.println(m[i].toString());
		System.out.println("declared methods");
		m = c.getDeclaredMethods();
		for (int i = 0; i < m.length; i++)
			System.out.println(m[i].toString());
		
		c = C1.class;
		m = c.getDeclaredMethods();
		for (int i = 0; i < m.length; i++)
			System.out.println(m[i].toString());
	}
}

// Sort Output

/* Expected Output:
all methods
declared methods
null
public abstract void ReflectInterfaces$C1.if1Method()
public abstract void ReflectInterfaces$C1.if1MethodNP()
public abstract void ReflectInterfaces$C1.if1MethodTH() throws java.lang.Throwable
public abstract void ReflectInterfaces$C1.if2Method()
public abstract void ReflectInterfaces$C1.if2MethodNP()
public abstract void ReflectInterfaces$C1.if2MethodTH() throws java.lang.Throwable
public abstract void ReflectInterfaces$If1.if1Method()
public abstract void ReflectInterfaces$If1.if1MethodNP()
public abstract void ReflectInterfaces$If1.if1MethodTH() throws java.lang.Throwable
public abstract void ReflectInterfaces$If2.if2Method()
public abstract void ReflectInterfaces$If2.if2Method()
public abstract void ReflectInterfaces$If2.if2MethodNP()
public abstract void ReflectInterfaces$If2.if2MethodNP()
public abstract void ReflectInterfaces$If2.if2MethodTH() throws java.lang.Throwable
public abstract void ReflectInterfaces$If2.if2MethodTH() throws java.lang.Throwable
*/
