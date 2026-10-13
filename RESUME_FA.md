# ادامه از بستهٔ ۱۱

بسته تجمعی است؛ هدف تمام خانواده‌های Arduino و فاز ۲ همچنان در حال تکمیل است.

جدید: src/cts/ArduinoMqttAdapter.h آداپتر اختیاری ArduinoMqttClient0.1.8. MQTT واقعی تزریق می‌شود؛ callback فقط بایت‌ها را در NetworkInbox صف می‌کند، publishReply پس از PUBACK پاسخ CommandSession را آزاد می‌کند. retained/topic/QoS/اندازه/خواندن و نوشتن ناقص بررسی می‌شوند. polling-only اولیه به علت دورریختن فرمان هم‌زمان در upstream کنار گذاشته شد؛ regression هم‌زمانی با خود MqttClient.cpp اجرا می‌شود.

وابستگی خارجی LGPL کپی نشده؛ SHA4699dda841c08b01036cd14412330bddc6c94edb. دستور tools/check_arduino_mqtt.py و فایل docs/ARDUINO_MQTT_ADAPTER.md را بخوان. تست shim بومی به معنی کامپایل روی برد یا TLS واقعی نیست. نتایج در VALIDATION.md.

backend همگام و بالقوه blocking است. backend غیرمسدودکننده، ساعت/Client امن واقعی هر خانواده، هماهنگ‌کنندهٔ hello/پاسخ معلق در reconnect، قطع حین ارسال و آزمون RAM/برد هنوز باقی است. پیش از اعلام کامل‌شدن فاز ۲ این‌ها باید بسته شوند. سپس scheduler و درایورهای واقعی فاز ۳.

میزبان آزمون همچنان74c7b3f15c2d093da03e392094814d63a4997c2c و origin/main مشاهده‌شدهfa7e7e995afe3ad7c0970ed783948d0336ac91f3 است. هیچ push یا تغییر source میزبان انجام نشده.
