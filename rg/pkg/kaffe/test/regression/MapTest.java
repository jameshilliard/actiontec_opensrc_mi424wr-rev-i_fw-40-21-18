import java.util.*;

class MapTest {
  static long seed = -1;
  static int num = 30;

  public static void main (String[] args) throws Throwable {
    try {
      switch (args.length) {
	case 2:
	  seed = Long.parseLong(args[1]);
	  /* fall through */
	case 1:
	  num = Integer.parseInt(args[0]);
	  /* fall through */
	case 0:
	  break;
	default:
	  throw new Exception("Usage: SortTest [#elem [randseed]]");
      }
      if (seed == -1) {
	seed = System.currentTimeMillis();
      }
      final Random r = new Random(seed);

      // Test basic Map properties of HashMap
      HashMap hm = new HashMap();
      checkMap(hm, r, num);

      // Test basic Map properties of TreeMap
      TreeMap tm = new TreeMap(new Comparator() {
	  public int compare(Object x, Object y) {
	    return (x == null) ? ((y == null) ? 0 : -1) :
		   (y == null) ? 1 : ((Comparable)x).compareTo(y);
	  }
	});
      checkMap(tm, r, num);

      check(tm.entrySet().equals(new HashMap(tm).entrySet()));
      check(hm.entrySet().equals(new TreeMap(hm).entrySet()));
    } catch (Throwable t) {
      System.out.println("FAILURE: reproduce with these arguments: "
	+ num + " " + seed);
      throw t;
    }
  }

  // Check basic Map properties
  static void checkMap(Map map, Random r, int num) throws Exception {

    System.out.println("Checking " +map.getClass());
    check(map.isEmpty());

    // Shadow copy of map
    int[] ary = new int[num];
    int[] ary2 = new int[num];
    Arrays.fill(ary, -1);

    // Fill in map
    int size = 0;
    for (int i = 0; i < num; i++) {
      int x = r.nextInt(num);
      ary[x] = i;
      if (map.put(new Integer(x), new Integer(i)) == null) {
	size++;
      }
      checkSorted(map);
      check(map.size() == size);
    }
    check(!map.isEmpty());

    // Check keys and their mappings
    for (int i = 0; i < num; i++) {
      if (ary[i] == -1) {
	check(!map.containsKey(new Integer(i)));
	check(map.get(new Integer(i)) == null);
      } else {
	check(map.containsKey(new Integer(i)));
	check(map.get(new Integer(i)).equals(new Integer(ary[i])));
      }
    }

    // Check values
    for (int i = 0; i < num; i++) {
      if (ary[i] != -1) {
	check(map.containsValue(new Integer(ary[i])));
      }
    }
    check(!map.containsValue(new Integer(-1)));
    check(!map.containsValue(new Integer(num)));

    // Check keySet();
    Set ks = map.keySet();
    Iterator tempi;
    for (tempi = ks.iterator(); tempi.hasNext(); ) {
      check(ary[((Integer)tempi.next()).intValue()] != -1);
    }
    boolean exok = false;
    try {
      tempi.next();
    } catch (NoSuchElementException _) {
      exok = true;
    }
    check(exok);
    for (int i = 0; i < num; i++) {
      if (ary[i] == -1) {
	check(!ks.contains(new Integer(i)));
      } else {
	check(ks.contains(new Integer(i)));
      }
    }

    // Check clone(), equals(), and hashCode()
    int hash = map.hashCode();
    check(map.equals(map));
    Map clone = (Map)map.getClass().getMethod("clone", null).invoke(map, null);
    check(map.equals(clone));
    checkSorted(clone);
    clone.put(new Integer(-1), new Integer(-1));
    check(!map.equals(clone));
    checkSorted(clone);
    clone.remove(new Integer(-1));
    check(map.equals(clone));
    check(clone.hashCode() == hash);
    checkSorted(map);

    // Check map can handle null keys and values
    map.put(null, new Integer(-1));
    check(map.containsKey(null));
    check(map.get(null).equals(new Integer(-1)));
    checkSorted(map);
    map.remove(null);
    check(!map.containsKey(null));
    checkSorted(map);
    map.put(new Integer(-1), null);
    checkSorted(map);
    check(map.containsValue(null));
    checkSorted(map);
    map.remove(new Integer(-1));
    check(!map.containsValue(null));
    checkSorted(map);

    // Check entrySet()
    Set es = map.entrySet();
    Arrays.fill(ary2, -1);
    Map.Entry laste = null;
    Iterator esi;
    for (esi = es.iterator(); esi.hasNext(); ) {
      Map.Entry e = (Map.Entry)esi.next();
      check(ary[((Integer)e.getKey()).intValue()]
			== ((Integer)e.getValue()).intValue());
      ary2[((Integer)e.getKey()).intValue()] =
		((Integer)e.getValue()).intValue();
      laste = e;
    }
    check(Arrays.equals(ary, ary2));

    // Check entrySet()'s iterator's remove() works and is backed by the map
    int key = ((Integer)laste.getKey()).intValue();
    int val = ((Integer)laste.getValue()).intValue();
    int osize = map.size();
    esi.remove();
    check(!map.containsKey(new Integer(key)));
    check(map.size() == osize - 1);
    checkSorted(map);
    boolean rmok = false;
    try {
      esi.remove();
    } catch (IllegalStateException _) {
      rmok = true;
    }
    check(rmok);
    checkSorted(map);
    map.put(new Integer(key), new Integer(val));
    check(map.containsKey(new Integer(key)));
    check(map.containsValue(new Integer(val)));
    check(map.size() == osize);
    checkSorted(map);

    // Check values() method
    Collection vals = map.values();
    Arrays.fill(ary2, -1);
    for (Iterator i = es.iterator(); i.hasNext(); ) {
      Map.Entry e = (Map.Entry)i.next();
      ary2[((Integer)e.getValue()).intValue()] = 1;
    }
    for (int i = 0; i < num; i++) {
      if (ary[i] != -1) {
	check(ary2[ary[i]] == 1);
      }
    }
    int[] ary3 = new int[num];
    System.arraycopy(ary, 0, ary3, 0, num);
    Arrays.sort(ary3);
    for (int i = 0; i < num; i++) {
      if (ary2[i] == 1) {
	check(Arrays.binarySearch(ary3, i) >= 0);
      }
    }

    // Check entrySet clear()
    es.clear();
    check(map.isEmpty());
    check(map.size() == 0);
    check(!map.values().iterator().hasNext());
    check(!map.keySet().iterator().hasNext());
    check(!map.entrySet().iterator().hasNext());
    checkSorted(map);

    // Put back all entries
    for (int i = 0; i < num; i++) {
      if (ary[i] != -1) {
	map.put(new Integer(i), new Integer(ary[i]));
	checkSorted(map);
      }
    }
    check(map.equals(clone));
  }

  // Check SortedMap properties
  static void checkSorted(Map map0) throws Exception {

    // See if its really a SortedMap
    if (!(map0 instanceof SortedMap)) {
      return;
    }
    SortedMap map = (SortedMap)map0;

    // Check sortedness
    checkIsSorted(map);

    // This will test SortedMap.subList() once it is implemented
    try {

      int count = 0;
      int ary[] = new int[map.size()];
      for (Iterator i = map.keySet().iterator(); i.hasNext(); ) {
	Integer next = (Integer)i.next();
	ary[count++] = (next == null) ? -1 : next.intValue();
      }
      check(count == map.size());

      Integer halfKey = new Integer(count == 0 ? 0 : ary[count / 2]);
      SortedMap map2 = map.tailMap(halfKey);
      SortedMap map3 = new TreeMap(map2);

      for (Iterator i = map2.entrySet().iterator(); i.hasNext(); ) {
	i.next();
	i.remove();
	checkIsSorted(map);
	checkIsSorted(map2);
      }
      try {
	map2.firstKey();
	check(false);
      } catch (NoSuchElementException e) {
      }
      check(map.size() == count / 2);
      check(map.get(halfKey) == null);

      int index = 0;
      for (Iterator i = map.keySet().iterator(); i.hasNext(); index++) {
	Integer next = (Integer)i.next();
	check(map.comparator().compare(next, halfKey) < 0);
	check(ary[index] == ((next == null) ? -1 : next.intValue()));
      }

      if (count > 1) {
	try {
	  check(((Integer)map.firstKey()).intValue() == ary[0]);
	} catch (NullPointerException e) {
	  check(ary[0] == -1);
	}
	try {
	  check(((Integer)map.lastKey()).intValue() == ary[count / 2 - 1]);
	} catch (NullPointerException e) {
	  check(ary[count / 2 - 1] == -1);
	}
      }

      for (Iterator i = map3.entrySet().iterator(); i.hasNext(); ) {
	Map.Entry e = (Map.Entry)i.next();
	map2.put(e.getKey(), e.getValue());
	checkIsSorted(map);
	checkIsSorted(map2);
      }

      if (count > 0) {
	try {
	  check(((Integer)map.firstKey()).intValue() == ary[0]);
	} catch (NullPointerException e) {
	  check(ary[0] == -1);
	}
	try {
	  check(((Integer)map.lastKey()).intValue() == ary[count - 1]);
	} catch (NullPointerException e) {
	  check(ary[count - 1] == -1);
	}
      }

      index = 0;
      for (Iterator i = map.keySet().iterator(); i.hasNext(); ) {
	Integer next = (Integer)i.next();
	check(ary[index++] == ((next == null) ? -1 : next.intValue()));
      }
    } catch (Error e) {
      if (!e.getClass().getName().equals("kaffe.util.NotImplemented")) {
	throw e;
      }
    }
  }

  static void checkIsSorted(SortedMap map) {
    Comparator c = map.comparator();
    Object ary[] = new Object[map.size()];
    int count = 0;
    for (Iterator i = map.keySet().iterator(); i.hasNext(); ) {
      ary[count++] = i.next();
    }
    for (int i = count - 1; i > 0; i--)
      check(c.compare(ary[i - 1], ary[i]) < 0);
  }

  public static void check(boolean truth) {
    if (!truth) {
      throw new Error("assertion failure");
    }
  }
}

/* Expected Output:
Checking class java.util.HashMap
Checking class java.util.TreeMap
*/

