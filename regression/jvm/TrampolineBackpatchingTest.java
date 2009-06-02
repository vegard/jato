/*
 * Copyright (C) 2009 Tomasz Grabiec
 *
 * This file is released under the GPL version 2 with the following
 * clarification and special exception:
 *
 *     Linking this library statically or dynamically with other modules is
 *     making a combined work based on this library. Thus, the terms and
 *     conditions of the GNU General Public License cover the whole
 *     combination.
 *
 *     As a special exception, the copyright holders of this library give you
 *     permission to link this library with independent modules to produce an
 *     executable, regardless of the license terms of these independent
 *     modules, and to copy and distribute the resulting executable under terms
 *     of your choice, provided that you also meet, for each linked independent
 *     module, the terms and conditions of the license of that module. An
 *     independent module is a module which is not derived from or based on
 *     this library. If you modify this library, you may extend this exception
 *     to your version of the library, but you are not obligated to do so. If
 *     you do not wish to do so, delete this exception statement from your
 *     version.
 *
 * Please refer to the file LICENSE for details.
 */
package jvm;

/**
 * @author Tomasz Grabiec
 */
public class TrampolineBackpatchingTest extends TestCase {
    public static int staticMethod(int x) {
        return x+x;
    }

    private class A {
        public int virtualMethod(int x) {
            return x+x;
        }

        private int privateMethod(int x) {
            return x+x;
        }

        public int invokePrivate(int x) {
            return privateMethod(x);
        }
    }

    private class B extends A {
        public int invokeSuper(int x) {
            return super.virtualMethod(x);
        }

        public int invokeVirtual(int x) {
            return virtualMethod(x);
        }
    }

    public static void testInvokestatic() {
        assertEquals(staticMethod(4), 8);
    }

    public  void testInvokevirtual() {
        assertEquals(new A().virtualMethod(4), 8);
        assertEquals(new B().invokeVirtual(4), 8);
    }

    public  void testInvokespecial() {
        assertEquals(new A().invokePrivate(4), 8);
        assertEquals(new B().invokeSuper(4), 8);
    }

    public static void main(String [] args) {
        TrampolineBackpatchingTest t = new TrampolineBackpatchingTest();

        testInvokestatic();
        t.testInvokevirtual();
        t.testInvokespecial();

        /*
         * Run tests again to check if all call sites have
         * been properly fixed.
         */
        testInvokestatic();
        t.testInvokevirtual();
        t.testInvokespecial();

        exit();
    }
}
