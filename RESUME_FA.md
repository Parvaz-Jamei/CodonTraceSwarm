# ادامه از بستهٔ ۰۶

زیپ تجمعی است و برنامهٔ هدف V2 در planning/Target_Plan_V2.zip باقی است. پس از پاک‌شدن محیط، همین زیپ را بده و بگو «از RESUME_FA.md ادامه بده».

جدید: NetworkInbox با کپی محدود bytes، topic دقیق، اتصال topic به نشست واقعی، دریافت قطعه‌ای، generation اتصال و مدیریت پرشدن/قطع صف. هیچ اجرا/parse/publish داخل push انجام نمی‌شود. pollOne جداست و بعد session.poll اجرا را انجام می‌دهد.

تستر مستقل یک نقص scope پیدا کرد؛ اصلاح شد و ۸۷ آزمون مستقل اضافه شد. بررسی شبکهٔ واقعی یا TLS هنوز انجام نشده. شرح مرزها در docs/NETWORK_INBOX.md و نتیجه در VALIDATION.md است.

قدم بعد: adapter واقعی ESP32 ساعت و MQTT/TLS با CA/hostname، ACL دو node و تست broker، serializing دسترسی taskها، publish مرتب و reconnect/backoff. سپس reply رد admission و retry-window مطابق هدف، درایور/FDC و scheduler، trace/replay و پژوهش. جدول docs/TARGET_PLAN_STATUS_FA.md معیار ادامه است. بستهٔ ششم پایان فاز پنجم نیست.

مرجع میزبان 74c7b3f15c2d093da03e392094814d63a4997c2c؛ قبل از تغییر fetch و تحقیق کد/منابع اصلی. تست: python3 tools/check.py --host /path/to/genesis. دادهٔ حسگر همهٔ fixtureها ساختگی است. این نسخه را دانلود و نگه دار.
