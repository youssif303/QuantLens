#include "AppController.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("QuantLens");
    QGuiApplication::setApplicationName("QuantLens");
    QGuiApplication::setApplicationVersion("1.0.0");

    QCommandLineParser cli;
    cli.setApplicationDescription("QuantLens - market analysis and statistical forecasts (not financial advice)");
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption offline("offline", "Use bundled/cached data only; no network.");
    QCommandLineOption shot("screenshot", "Save a screenshot to <file> and quit.", "file");
    QCommandLineOption delay("delay", "Milliseconds to wait before the screenshot (default 4000).", "ms", "4000");
    QCommandLineOption symbol("symbol", "Symbol to select at start.", "symbol");
    QCommandLineOption interval("interval", "Candle interval to select at start (e.g. 1m, 1h, 1d).", "id");
    QCommandLineOption currency("currency", "Display currency (USD or EUR).", "c");
    cli.addOptions({offline, shot, delay, symbol, interval, currency});
    cli.process(app);

    ql::AppController controller(cli.isSet(offline));
    if (cli.isSet(symbol)) controller.select(cli.value(symbol));
    if (cli.isSet(currency)) controller.setCurrency(cli.value(currency));
    if (cli.isSet(interval)) controller.setInterval(cli.value(interval));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(1); },
                     Qt::QueuedConnection);
    engine.loadFromModule("QuantLens", "Main");

    if (cli.isSet(shot)) {
        const QString file = cli.value(shot);
        QTimer::singleShot(cli.value(delay).toInt(), &app, [&engine, file]() {
            auto* w = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
            const bool ok = w && w->grabWindow().save(file);
            QCoreApplication::exit(ok ? 0 : 2);
        });
    }
    return app.exec();
}
