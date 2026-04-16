	Dbgen	Tpchgen	Schengen (наш)
Формат вывода	Генерация только в tbl/csv. Лишняя конвертация	Прямая генерация	Прямая генерация
Интеграция с хранилищами	Не поддерживает прямую запись  в холодные хранилища. Только локальный диск	Не поддерживает прямую запись  в холодные хранилища. Только локальный диск	Поддерживает прямую запись во все S3-совместимые хранилища + локальный диск
Производительность	Медленный, однопоточная генарация	Быстрый, параллельная генарация партиций и таблиц	Быстрый, параллельная генарация партиций и таблиц
Лицензия	Закрытая TPC	Открытая Apache 2.0	Открытая
Прямая генерация данных в формате Arrow	Нет	Есть	Есть
Язык	С	Rust	C++




Существующие решения либо устарели по формату, либо не учитывают всем инфраструктурным требованиям.  Мой проект закрывает потребность в быстром и детерминированном генераторе TPC‑H, который умеет работать с современными колоночными форматами для хранения данных и позволяет работать с S3‑хранилищами напрямую, сохраняя детерменированность и имеет открытую лицензию для использования в коммерческих продуктах



Обязательно Mermaid диаграммы:

Модель Предметной Области
Диаграмма прецендентов
4К диаграмма контекста
4К ДИАГРАММА КОНТЕЙНЕРА
4К ДИАГРАММА КОМПОНЕНТ


Список литературы:

1.	Transaction Processing Performance Council (TPC). TPC Benchmark™ H Standard Specification, Version 2.17.1. https://www.tpc.org/tpc_documents_current_versions/pdf/tpc-h_v2.17.1.pdf 
2.	D. J. DeWitt and J. Gray, “Parallel Database Systems: The Future of High Performance Database Systemshttps://pesc.coppe.ufrj.br/~ines/courses/TEPP/pcomp-25-13.26-99.pdf 
3.	Apache Arrow: A Cross-Language Development Platform for In-Memory Columnar Datahttps://arrow.apache.org/ 
4.	TBL Format Specificationhttps://en.wikipedia.org/wiki/Troff
5.	Apache Parquet Format Specificationhttps://en.wikipedia.org/wiki/Apache_Parquet 
6.	Apache ORC Format Specificationhttps://en.wikipedia.org/wiki/Apache_ORC
7.	Apache Iceberg Format Specificationhttps://en.wikipedia.org/wiki/Apache_Iceberg
8.	Apache Lance Format Specificationhttps://github.com/lancedb/lance 
9.	Apache Vortex Format Specificationhttps://github.com/spiraldb/vortex 

