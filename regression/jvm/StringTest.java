package jvm;

/**
 * @author Vegard Nossum
 */
public class StringTest extends TestCase {
    public static void testUnicode() {
        String s = "\u00c6\u00d8\u00e5";
        assertEquals(s.length(), 3);

        String t = "\u306a\u308b\u3068";
        assertEquals(t.length(), 3);
    }

    public static void testStringConcatenation() {
        String a = "123";
        String b = "abcd";

        assertObjectEquals("123abcd", a + b);
    }

    public static void main(String args[]) {
        testUnicode();
        testStringConcatenation();
    }
}
