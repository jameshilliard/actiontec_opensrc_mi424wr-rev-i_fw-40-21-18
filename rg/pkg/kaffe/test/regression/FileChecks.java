import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;

public class FileChecks 
{
	public static void main(String[] argv) 
	{
		try
		{
			FileInputStream     fis  = new FileInputStream(".");
			System.out.println("1 FAIL! Should throw java.io.FileNotFoundException!");
			System.exit(1);
		}
		catch (FileNotFoundException e)
		{
			System.out.println("1 Success.");
		}

		try
		{
			File                file = new File(".");
			FileInputStream     fis  = new FileInputStream(file);
			System.out.println("2 FAIL! Should throw java.io.FileNotFoundException!");
			System.exit(1);
		}
		catch (FileNotFoundException e)
		{
			System.out.println("2 Success.");
		}

		try
		{
			FileOutputStream     fis  = new FileOutputStream(".");
			System.out.println("3 FAIL! Should throw java.io.FileNotFoundException!");
			System.exit(1);
		}
		catch (FileNotFoundException e)
		{
			System.out.println("3 Success.");
		}

		try
		{
			File                file = new File(".");
			FileOutputStream     fis  = new FileOutputStream(file);
			System.out.println("4 FAIL! Should throw java.io.FileNotFoundException!");
			System.exit(1);
		}
		catch (FileNotFoundException e)
		{
			System.out.println("4 Success.");
		}

		try
		{
			File                file = new File("FileChecks.class"); // should exist
			FileInputStream     fis  = new FileInputStream(file);
			System.out.println("5 Success.");
		}
		catch (FileNotFoundException e)
		{
			System.out.println("5 FAIL! Should not get: " +e);
		}

	}
}

/* Expected Output:
1 Success.
2 Success.
3 Success.
4 Success.
5 Success.
*/
