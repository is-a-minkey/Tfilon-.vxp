import java.lang.reflect.*; import java.io.*; import java.util.*;
public class SunHarness {
    public static void main(String[] a) throws Exception {
        Class<?> T = Class.forName("t"), R = Class.forName("r"), Qc = Class.forName("q"), S = Class.forName("s");
        S.getMethod("a").invoke(null);
                Method m1=null,m2=null,m3=null,m4=null,m5=null,m6=null;
        for (Method m : T.getDeclaredMethods()) {
            Class<?>[] p = m.getParameterTypes();
            if (m.getName().equals("a") && false) {}
        }
        // original names are obfuscated: look up by (name, params)
        Method latM = find(T, "a", int[].class, boolean.class), lonM = find(T, "b", int[].class, boolean.class);
        Method elevM = find(T, "a", int.class, void.class), tzM = find(T, "a", int[].class, void.class);
        Method sunsetM = find(T, "b", R, int.class), sunriseM = find(T, "a", R, int.class);
        Field modeF = null; for (Field ff : T.getDeclaredFields()) if (ff.getName().equals("a") && ff.getType()==int.class) { ff.setAccessible(true); modeF = ff; }
        Constructor<?> qn = Qc.getConstructor(int.class, int.class, int.class);
        Method rA = R.getMethod("a", Qc); Field rint = null;
        for (Field f : R.getDeclaredFields()) if (f.getName().equals("a") && f.getType()==int.class) { f.setAccessible(true); rint = f; }
        BufferedReader br = new BufferedReader(new FileReader("/tmp/ref/cities.txt")); String line; int idx = 0;
        PrintStream out = new PrintStream(new BufferedOutputStream(System.out, 1<<16));
        while ((line = br.readLine()) != null) {
            String[] f = line.trim().split("\\s+");
            int[] lat = {Integer.parseInt(f[0]),Integer.parseInt(f[1]),Integer.parseInt(f[2]),Integer.parseInt(f[3])};
            int[] lon = {Integer.parseInt(f[4]),Integer.parseInt(f[5]),Integer.parseInt(f[6]),Integer.parseInt(f[7])};
            double tz = Double.parseDouble(f[8]); int elev = Integer.parseInt(f[9]);
            for (int summer = 0; summer < 2; summer++) {
                Object t = T.getConstructor().newInstance();
                latM.invoke(t, (Object) lat); lonM.invoke(t, (Object) lon); elevM.invoke(t, elev);
                double d2 = tz; int neg = 0; if (d2 < 0) { d2 = -d2; neg = 1; }
                int[] tzA = { (int) d2, (int) (60.0 * (d2 - (double)((int) d2))), neg };
                modeF.setInt(t, summer);
                tzM.invoke(t, (Object) tzA);
                for (int mon = 0; mon < 12; mon += 1) for (int day : new int[]{3, 21}) {
                    Object r = R.getConstructor().newInstance(); rA.invoke(r, qn.newInstance(day, mon, 2026));
                    int abs = rint.getInt(r);
                    out.println(idx + " " + summer + " " + abs + " " + sunriseM.invoke(t, r) + " " + sunsetM.invoke(t, r));
                }
            }
            idx++;
        }
        out.flush();
    }
    static Method find(Class<?> c, String name, Class<?> p0, Class<?> ret) throws Exception {
        for (Method m : c.getDeclaredMethods()) if (m.getName().equals(name) && m.getParameterCount()==1 && m.getParameterTypes()[0]==p0 && m.getReturnType()==ret) { m.setAccessible(true); return m; }
        throw new RuntimeException("no method " + name + " " + p0 + " " + ret);
    }
}
