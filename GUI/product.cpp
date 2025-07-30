#include "product.h"
#include "ui_product.h"

Product::Product(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Product)
{
    ui->setupUi(this);
}

Product::~Product()
{
    delete ui;
}

void Product::on_searchBtn_clicked()
{
    QString id = ui->prodId->text().trimmed();
        searchProduct(id); // fct created
}
void Product::searchProduct(const QString &id){
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("C:/Users/moham/OneDrive/Documents/QT/database/supermarketdb.db");
            if (db.open()) {
                 QSqlQuery query(db);
                 query.prepare("SELECT * FROM Products WHERE id = :id");
                 query.bindValue(":id",id);

                 if (query.exec()) {
                     if (query.next()) {

                            ui->prodName->setText(query.value(1).toString());
                               ui->prodPrice->setText(query.value(2).toString());
                         ui->prodDetails->setText(query.value(3).toString());

                         productFound = true;
                         ui->panel->setText("Product found.");
                     } else {
                         ui->panel->setText("Product not found. Enter details to add it.");

                         productFound = false;
                         ui->prodName->clear();
                         ui->prodPrice->clear();
                         ui->prodDetails->clear();
                     }
                 } else {
                     ui->panel->setText("Query error");
                 }
             } else {
                 ui->panel->setText("Failed to connect to the database.");
             }
}



void Product::on_clearBtn_clicked()
{
    ui->prodName->clear();
    ui->prodPrice->clear();
    ui->prodDetails->clear();
}


void Product::on_addBtn_clicked()
{
    if (!productFound) {
            QString id = ui->prodId->text().trimmed();
            QString name = ui->prodName->text().trimmed();
            QString price = ui->prodPrice->text().trimmed();
            QString details = ui->prodDetails->text().trimmed();

            addProduct(id, name, price, details);
        } else {
            ui->panel->setText("Product already found. Modify details as needed.");
        }
    }


void Product::addProduct(const QString &id, const QString &name, const QString &price, const QString &details)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("C:/Users/moham/OneDrive/Documents/QT/database/supermarketdb.db");

    if (db.open()) {
        QSqlQuery query;
        query.prepare("INSERT INTO products (id, name, price, details) VALUES (:id, :name, :price, :details)");
        query.bindValue(":id", id);
        query.bindValue(":name", name);
        query.bindValue(":price", price);
        query.bindValue(":details", details);

        if (query.exec()) {
            ui->panel->setText("Product added successfully.");
            productFound = true;
        } else {
            ui->panel->setText("Failed to add product: " + query.lastError().text());
        }
    } else {
        ui->panel->setText("Failed to connect to database.");
    }
}



void Product::on_saveBtn_clicked()
{
    if (productFound) {
           QString id = ui->prodId->text().trimmed();
           QString name = ui->prodName->text().trimmed();
           QString price = ui->prodPrice->text().trimmed();
           QString details = ui->prodDetails->text().trimmed();

           updateProduct(id, name, price, details);
       }
}

void Product::updateProduct(const QString &id, const QString &name, const QString &price, const QString &details)
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("C:/Users/moham/OneDrive/Documents/QT/database/supermarketdb.db");

    if (db.open()) {
        QSqlQuery query;
        query.prepare("UPDATE Products SET name = :name, price = :price, details = :details WHERE id = :id");
        query.bindValue(":id", id);
        query.bindValue(":name", name);
        query.bindValue(":price", price);
        query.bindValue(":details", details);

        if (query.exec()) {
            ui->panel->setText("Product details updated.");
        } else {
            ui->panel->setText("Failed to update product: " + query.lastError().text());
        }
    } else {
        ui->panel->setText("Failed to connect to database.");
    }
}




void Product::on_cancelBtn_clicked()
{
    this->close();
}


void Product::on_deleteBtn_clicked()
{
    QString productId = ui->prodId->text().trimmed();

        if (productId.isEmpty()) {
            ui->panel->setText("Please enter a Product ID.");
            return;
        }


        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Delete Product",
                                      "Are you sure you want to delete this product?",
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            QSqlQuery query;
            query.prepare("DELETE FROM Products WHERE id = :id");
            query.bindValue(":id", productId);

            if (query.exec()) {
                ui->panel->setText("Product deleted successfully.");
                ui->prodId->clear();
                ui->prodName->clear();
                ui->prodPrice->clear();
                ui->prodDetails->clear();
            } else {
                ui->panel->setText("Error deleting product: " + query.lastError().text());
            }
        }
}


void Product::on_allProducts_clicked()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
       db.setDatabaseName("C:/Users/moham/OneDrive/Documents/QT/database/supermarketdb.db");
        if (!db.isOpen()) {
            if (!db.open()) {
                QMessageBox::critical(this, "Database Error", "Failed to open the database: " + db.lastError().text());
                return;
            }
        }

        QSqlQuery query;
        query.prepare("SELECT id, name, price, details FROM products");

        if (!query.exec()) {
            QMessageBox::critical(this, "Database Error", "Failed to retrieve products: " + query.lastError().text());
            return;
        }

        QString productList;
        while (query.next()) {
            QString id = query.value(0).toString();
            QString name = query.value(1).toString();
            QString price = query.value(2).toString();
            QString details = query.value(3).toString();

            productList += "ID: " + id + "\n";
            productList += "Name: " + name + "\n";
            productList += "Price: " + price + "\n";
            productList += "Details: " + details + "\n";
            productList += "---------------------------\n";
        }

        if (productList.isEmpty()) {
            productList = "No products found in the database.";
        }

        QDialog *dialog = new QDialog(this);
        dialog->setWindowTitle("All Products");
        dialog->resize(400, 500);

        QTextEdit *textEdit = new QTextEdit(dialog);
        textEdit->setText(productList);
        textEdit->setReadOnly(true);

        QVBoxLayout *layout = new QVBoxLayout();
        layout->addWidget(textEdit);
        dialog->setLayout(layout);

        dialog->exec();  // Show the dialog
}





