import java.lang.reflect.*;
public class Harness {
    static Field fld(Class<?> c, String name, Class<?> type) throws Exception {
        for (Field f : c.getDeclaredFields()) if (f.getName().equals(name) && f.getType() == type) { f.setAccessible(true); return f; }
        throw new RuntimeException("no field " + name + " " + type);
    }
    public static void main(String[] args) throws Exception {
        int y0 = Integer.parseInt(args[0]), y1 = Integer.parseInt(args[1]);
        Class<?> S = Class.forName("s"), R = Class.forName("r"), Qc = Class.forName("q"), B = Class.forName("b");
        S.getMethod("a").invoke(null);
        Constructor<?> qn = Qc.getConstructor(int.class, int.class, int.class);
        Constructor<?> rn = R.getConstructor();
        Method rA = R.getMethod("a", Qc);                  // boolean a(q)  Gregorian -> sets everything
        Method rAq = R.getMethod("a", Qc, boolean.class);  // boolean a(q, boolean) Hebrew -> abs (clamped)
        Method bLeap = null;
        for (Method m : B.getDeclaredMethods()) if (m.getName().equals("a") && m.getParameterCount()==1 && m.getParameterTypes()[0]==int.class && m.getReturnType()==boolean.class) bLeap = m;
        Field rqa = fld(R, "a", Qc), rqb = fld(R, "b", Qc), rint = fld(R, "a", int.class);
        Field qa = fld(Qc, "a", int.class), qb = fld(Qc, "b", int.class), qc = fld(Qc, "c", int.class);
        java.io.PrintStream out = new java.io.PrintStream(new java.io.BufferedOutputStream(System.out, 1<<16));
        java.util.Calendar c = java.util.Calendar.getInstance(); c.clear(); c.set(y0, 0, 1);
        while (c.get(java.util.Calendar.YEAR) <= y1) {
            int gy = c.get(java.util.Calendar.YEAR), gm = c.get(java.util.Calendar.MONTH), gd = c.get(java.util.Calendar.DAY_OF_MONTH);
            Object x = rn.newInstance();
            boolean ok = (Boolean) rA.invoke(x, qn.newInstance(gd, gm, gy));
            if (!ok) { out.println("FAIL " + gy + " " + gm + " " + gd); }
            else {
                Object hq = rqa.get(x), gq = rqb.get(x);
                int abs = rint.getInt(x), hd = qa.getInt(hq), hm = qb.getInt(hq), hy = qc.getInt(hq);
                boolean leap = (Boolean) bLeap.invoke(null, hy);
                Object r2 = rn.newInstance();
                rAq.invoke(r2, qn.newInstance(30, 8, hy), true);
                boolean k30 = qa.getInt(rqa.get(r2)) == 30;
                int G_y = qc.getInt(gq), G_m = qb.getInt(gq), G_d = qa.getInt(gq);
                out.println(gy+" "+gm+" "+gd+" "+abs+" "+hd+" "+hm+" "+hy+" "
                    + Long.toHexString(FlagsRef.flags(abs,hd,hm,hy,G_y,G_m,G_d,leap,k30,false,1)) + " "
                    + Long.toHexString(FlagsRef.flags(abs,hd,hm,hy,G_y,G_m,G_d,leap,k30,true,3)) + " "
                    + Long.toHexString(FlagsRef.flags(abs,hd,hm,hy,G_y,G_m,G_d,leap,k30,false,2)));
            }
            c.add(java.util.Calendar.DAY_OF_MONTH, 1);
        }
        out.flush();
    }
}
