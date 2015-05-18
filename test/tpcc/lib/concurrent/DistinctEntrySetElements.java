package ddbt.tpcc.lib.concurrent;

/*

 * @test

 * @summary Sets from Map.entrySet() return distinct objects for each Entry

 */


import ddbt.tpcc.lib.concurrent.ConcurrentSHMap;

import java.util.HashSet;

import java.util.Map;

import java.util.Set;


public class DistinctEntrySetElements {

  public static void main(String[] args) throws Exception {

    final ConcurrentSHMap<String, String> ConcurrentSHMap = new ConcurrentSHMap<>();

    String[][] map = new String[][] {
      {"One", "Un"},
      {"Two", "Deux"},
      {"Three", "Trois"}
    };

    for(int i = 0; i < map.length; ++i) {
      ConcurrentSHMap.put(map[i][0], map[i][1]);
    }


    Set<Map.Entry<String, String>> entrySet = ConcurrentSHMap.entrySet();

    HashSet<Map.Entry<String, String>> hashSet = new HashSet<>(entrySet);


    if (false == hashSet.equals(entrySet)) {

      throw new RuntimeException("Test FAILED: Sets are not equal.");

    }

    if (hashSet.hashCode() != entrySet.hashCode()) {

      throw new RuntimeException("Test FAILED: Set's hashcodes are not equal.");

    }

    for(int i = 0; i < map.length; ++i) {
      String res = ConcurrentSHMap.get(map[i][0]);
      if(!res.equals(map[i][1])) {
        throw new RuntimeException(String.format("Test FAILED: Entry Value for key=%s was wrong. (Actual=%s, Expected=%s)",map[i][0],res,map[i][1]));
      }
    }

  }

}
