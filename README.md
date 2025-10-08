✷ โปรแกรมจัดการการสั่งสินค้าของลูกค้าโดยสามารถกดรันได้ปกติเลย สามารถAdd SearchจากIDและจากชื่อสินค้า Updateข้อมูล และDeleteข้อมูล
โดยป้องกันการใส่ค่า: Idที่ซ้ำ, overflow, underflow, invalid-datatype, วันเวลาจริงตั้งแต่ปี2020-2025
✷ Unit-test ของทุกฟังชั่นสามารถรันได้เลยไม่ต้องแก้โค้ดอะไรในโปรแกรมหลัก
✷ E2E-test ทำงานเรียงไปโดยเริ่มจาก Add → SearchbyID → Searchbyproduct("Bolt") → Update("BoltX)" → Searchbyproduct again ("boltx") → Delete → Exit ซึ่งสามารถรันได้เลยเหมือนUnit-test
